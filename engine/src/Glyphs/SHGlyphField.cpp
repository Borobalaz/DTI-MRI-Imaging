#include "Glyphs/SHGlyphField.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

#include "foundation/log/EngineLog.h"
#include "SphericalHarmonics/RealSH.h"

namespace
{
  constexpr const char* kLogSource = "SHGlyphField";
  constexpr int kMinSubdivisionLevel = 1;
  constexpr int kMaxSubdivisionLevel = 5;

  // Basis values of every mesh direction at the volume's order: directions x coefficientCount,
  // row-major. Shared by all glyphs of one build.
  std::vector<double> EvaluateDirectionBasis(const std::vector<glm::vec3>& directions, int lmax)
  {
    const size_t coefficientCount = static_cast<size_t>(RealSH::CoefficientCount(lmax));
    std::vector<double> basis(directions.size() * coefficientCount);
    for (size_t i = 0; i < directions.size(); ++i)
    {
      RealSH::EvaluateBasis(glm::normalize(glm::dvec3(directions[i])), lmax, &basis[i * coefficientCount]);
    }
    return basis;
  }

  // Largest clamped radius max(sum c_k Y_k, 0) over the mesh directions; the shader divides by it
  // in normalized mode.
  float MaxPositiveRadius(const float* coefficients, size_t coefficientCount, const std::vector<double>& directionBasis)
  {
    double maxRadius = 0.0;
    for (size_t offset = 0; offset < directionBasis.size(); offset += coefficientCount)
    {
      double radius = 0.0;
      for (size_t k = 0; k < coefficientCount; ++k)
      {
        radius += static_cast<double>(coefficients[k]) * directionBasis[offset + k];
      }
      maxRadius = std::max(maxRadius, radius);
    }
    return static_cast<float>(maxRadius);
  }

  bool HasNonZeroCoefficient(const float* coefficients, size_t coefficientCount)
  {
    return std::any_of(coefficients, coefficients + coefficientCount, [](float value) { return value != 0.0f; });
  }
}

SHGlyphBuildResult BuildSHGlyphField(const SHVolume& volume,
                                     const SHGlyphBuildSettings& settings,
                                     bool rebuildMesh,
                                     const CancellationToken& cancellation)
{
  if (!volume.IsWellFormed())
  {
    throw std::runtime_error("SH volume is malformed");
  }

  const int stride = std::max(settings.stride, 1);
  const size_t coefficientCount = volume.CoefficientsPerVoxel();
  const std::vector<glm::vec3> directions = IcosphereGeometry::GenerateDirections(settings.subdivisionLevel);
  const std::vector<double> directionBasis = EvaluateDirectionBasis(directions, volume.lmax);

  // Same voxel -> object mapping as Volume: the unit cube scaled by extents / maxExtent. The
  // field's own transform keeps scale 1, so glyphs stay round for anisotropic voxels. The
  // unit-radius mesh is scaled to half a stride cell, so at shGlyph.scale 1 a normalized glyph
  // exactly touches its neighbours at any stride (stride changes glyph size as well as count).
  const glm::ivec3& dimensions = volume.dimensions;
  const glm::vec3 extents = glm::vec3(dimensions) * volume.spacing;
  const float maxExtent = std::max({extents.x, extents.y, extents.z, 1e-6f});
  const glm::vec3 objectExtents = extents / maxExtent;
  const float cellSize = std::min({volume.spacing.x, volume.spacing.y, volume.spacing.z}) / maxExtent;
  const float glyphRadius = 0.5f * static_cast<float>(stride) * cellSize;

  std::vector<glm::mat4> transforms;
  std::vector<float> maxRadii;
  std::vector<float> paddedCoefficients;

  const std::vector<float>* maskVoxels = volume.mask ? &volume.mask->GetVoxels() : nullptr;
  for (int z = 0; z < dimensions.z; z += stride)
  {
    cancellation.ThrowIfCancellationRequested();
    for (int y = 0; y < dimensions.y; y += stride)
    {
      for (int x = 0; x < dimensions.x; x += stride)
      {
        if (maskVoxels &&
            (*maskVoxels)[VolumeData::FlatIndex(x, y, z, dimensions.x, dimensions.y)] < settings.maskThreshold)
        {
          continue;
        }

        const float* coefficients = volume.VoxelCoefficients(x, y, z);
        if (!HasNonZeroCoefficient(coefficients, coefficientCount))
        {
          continue;
        }

        const glm::vec3 center = ((glm::vec3(x, y, z) + 0.5f) / glm::vec3(dimensions) - 0.5f) * objectExtents;
        transforms.push_back(glm::scale(glm::translate(glm::mat4(1.0f), center), glm::vec3(glyphRadius)));
        maxRadii.push_back(MaxPositiveRadius(coefficients, coefficientCount, directionBasis));
        paddedCoefficients.insert(paddedCoefficients.end(), coefficients, coefficients + coefficientCount);
        paddedCoefficients.insert(paddedCoefficients.end(),
                                  SHGlyphField::kCoefficientFloatsPerInstance - coefficientCount, 0.0f);
      }
    }
  }

  SHGlyphBuildResult result;
  result.glyphCount = transforms.size();
  result.instances.SetTransforms(std::move(transforms));
  const InstanceStreamId maxRadiusStream =
    result.instances.AddAttributeStream({SHGlyphField::kMaxRadiusLocation, 1});
  const InstanceStreamId coefficientStream = result.instances.AddBufferStream(
    {"instanceCoefficients", TextureFormat::RGBA32F, SHGlyphField::kCoefficientFloatsPerInstance});
  result.instances.SetStreamData(maxRadiusStream, std::move(maxRadii));
  result.instances.SetStreamData(coefficientStream, std::move(paddedCoefficients));

  if (rebuildMesh)
  {
    result.geometry = std::make_shared<IcosphereGeometry>(settings.subdivisionLevel);
    result.basisTable = RealSH::BuildBasisTable(result.geometry->GetDirections());
  }
  return result;
}

SHGlyphField::SHGlyphField(const std::string& id,
                           std::shared_ptr<const SHVolume> volume,
                           std::shared_ptr<Shader> glyphShader,
                           SHGlyphBuildSettings settings)
  : id(id),
    volume(std::move(volume)),
    material(std::make_shared<Material>(std::move(glyphShader))),
    settings(settings)
{
  material->SetBaseColor(glm::vec3(1.0f));
  material->SetRoughness(0.5f);
  material->SetMetallic(0.0f);

  if (!this->volume)
  {
    EngineLog::Error("SH glyph field '" + id + "' has no SH volume; it will draw nothing", kLogSource);
    return;
  }
  StartBuild();
}

void SHGlyphField::Apply(IUniformSink& shader) const
{
  shader.SetTexture(ComposeUniformName("shGlyph", "basis"), basisTexture.get(), kBasisTextureUnit);
  shader.SetInt(ComposeUniformName("shGlyph", "coefficientTexelsPerInstance"), kCoefficientTexelsPerInstance);
  shader.SetFloat(ComposeUniformName("shGlyph", "scale"), glyphScale);
  shader.SetBool(ComposeUniformName("shGlyph", "normalize"), normalizeGlyphs);
}

void SHGlyphField::BuildRenderProxy(RenderProxy& renderProxy) const
{
  // Nothing to draw until the first build has landed (or when it produced no glyphs).
  if (!visible || !geometry || !basisTexture || !instanceBuffer.IsValid())
  {
    renderProxy.visible = false;
    return;
  }

  MeshInstancedRenderCommand command;
  command.geometry = geometry;
  command.material = material;
  command.instances = &instanceBuffer;
  command.modelMatrix = transform.GetModelMatrix();
  command.frameUniforms = renderProxy.frameUniforms;
  command.frameUniforms.AddProvider(*this);

  MeshInstancedRenderBatch batch;
  batch.draws.push_back(std::move(command));
  renderProxy.visible = true;
  renderProxy.data.Append(std::move(batch));
}

void SHGlyphField::Update(float deltaTime)
{
  if (buildTask && buildTask->IsFinished())
  {
    std::unique_ptr<BackgroundTask<SHGlyphBuildResult>> finishedTask = std::move(buildTask);
    try
    {
      SHGlyphBuildResult result = finishedTask->TakeResult();
      // A result that finished despite a newer request is already stale.
      if (!rebuildPending)
      {
        InstallBuildResult(std::move(result));
      }
    }
    catch (const OperationCancelled&)
    {
      EngineLog::Message("SH glyph build for '" + id + "' cancelled; keeping the previous glyphs", kLogSource);
    }
    catch (const std::exception& error)
    {
      EngineLog::Error("SH glyph build for '" + id + "' failed: " + std::string(error.what()), kLogSource);
    }
  }

  // Starts only once the previous build has stopped, so the engine thread never waits on a join.
  if (rebuildPending && !buildTask)
  {
    StartBuild();
  }

  IUpdateable::Update(deltaTime);
}

void SHGlyphField::SetStride(int newStride)
{
  newStride = std::max(newStride, 1);
  if (newStride == settings.stride)
  {
    return;
  }
  settings.stride = newStride;
  RequestRebuild();
}

void SHGlyphField::SetMaskThreshold(float newThreshold)
{
  if (newThreshold == settings.maskThreshold)
  {
    return;
  }
  settings.maskThreshold = newThreshold;
  RequestRebuild();
}

void SHGlyphField::SetSubdivisionLevel(unsigned int newLevel)
{
  newLevel = static_cast<unsigned int>(std::clamp(static_cast<int>(newLevel), kMinSubdivisionLevel, kMaxSubdivisionLevel));
  if (newLevel == settings.subdivisionLevel)
  {
    return;
  }
  settings.subdivisionLevel = newLevel;
  RequestRebuild();
}

std::optional<float> SHGlyphField::CastRay(const glm::vec3&, const glm::vec3&) const
{
  return std::nullopt; // not pickable
}

std::vector<InspectFieldPtr> SHGlyphField::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields = transform.GetInspectFields();

  auto visibleField = MakeInspectField("visible", "Visible", "Rendering", InspectFieldType::Boolean, visible,
    [this]() -> InspectValue { return visible; },
    [this](const InspectValue &value)
  {
    if (const auto *boolean = std::get_if<bool>(&value)) visible = *boolean;
  });
  fields.push_back(visibleField);

  auto scaleField = MakeInspectField("glyphScale", "Scale", "Glyphs", InspectFieldType::Number, static_cast<double>(glyphScale),
    [this]() -> InspectValue { return static_cast<double>(glyphScale); },
    [this](const InspectValue &value)
  {
    if (const auto *number = std::get_if<double>(&value)) glyphScale = static_cast<float>(*number);
  });
  scaleField->minimum = 0.0;
  scaleField->maximum = 10.0;
  scaleField->step = 0.05;
  fields.push_back(scaleField);

  auto normalizeField = MakeInspectField("normalize", "Normalize", "Glyphs", InspectFieldType::Boolean, normalizeGlyphs,
    [this]() -> InspectValue { return normalizeGlyphs; },
    [this](const InspectValue &value)
  {
    if (const auto *boolean = std::get_if<bool>(&value)) normalizeGlyphs = *boolean;
  });
  fields.push_back(normalizeField);

  auto strideField = MakeInspectField("stride", "Stride", "Glyphs", InspectFieldType::Number, static_cast<double>(settings.stride),
    [this]() -> InspectValue { return static_cast<double>(settings.stride); },
    [this](const InspectValue &value)
  {
    if (const auto *number = std::get_if<double>(&value)) SetStride(static_cast<int>(*number));
  });
  strideField->minimum = 1.0;
  strideField->maximum = 32.0;
  strideField->step = 1.0;
  fields.push_back(strideField);

  auto thresholdField = MakeInspectField("maskThreshold", "Mask Threshold", "Glyphs", InspectFieldType::Number,
    static_cast<double>(settings.maskThreshold),
    [this]() -> InspectValue { return static_cast<double>(settings.maskThreshold); },
    [this](const InspectValue &value)
  {
    if (const auto *number = std::get_if<double>(&value)) SetMaskThreshold(static_cast<float>(*number));
  });
  thresholdField->minimum = 0.0;
  thresholdField->maximum = 1.0;
  thresholdField->step = 0.01;
  fields.push_back(thresholdField);

  auto subdivisionField = MakeInspectField("subdivisionLevel", "Subdivision Level", "Glyphs", InspectFieldType::Number,
    static_cast<double>(settings.subdivisionLevel),
    [this]() -> InspectValue { return static_cast<double>(settings.subdivisionLevel); },
    [this](const InspectValue &value)
  {
    if (const auto *number = std::get_if<double>(&value)) SetSubdivisionLevel(static_cast<unsigned int>(*number));
  });
  subdivisionField->minimum = kMinSubdivisionLevel;
  subdivisionField->maximum = kMaxSubdivisionLevel;
  subdivisionField->step = 1.0;
  fields.push_back(subdivisionField);

  auto glyphCountField = MakeInspectField("glyphCount", "Glyph Count", "Glyphs", InspectFieldType::Number,
    static_cast<double>(glyphCount),
    [this]() -> InspectValue { return static_cast<double>(glyphCount); });
  glyphCountField->readOnly = true;
  glyphCountField->maximum = 1e9;
  fields.push_back(glyphCountField);

  auto rebuildingField = MakeInspectField("rebuilding", "Rebuilding", "Glyphs", InspectFieldType::Boolean,
    buildTask != nullptr || rebuildPending,
    [this]() -> InspectValue { return buildTask != nullptr || rebuildPending; });
  rebuildingField->readOnly = true;
  fields.push_back(rebuildingField);

  return fields;
}

void SHGlyphField::RequestRebuild()
{
  if (!volume)
  {
    return;
  }

  rebuildPending = true;
  if (buildTask)
  {
    buildTask->Cancel();
  }
}

void SHGlyphField::StartBuild()
{
  rebuildPending = false;
  const bool rebuildMesh = !geometry || geometry->GetSubdivisionLevel() != settings.subdivisionLevel;

  // The task copies the shared_ptr and the settings, so later edits never race the worker.
  buildTask = std::make_unique<BackgroundTask<SHGlyphBuildResult>>(
    [buildVolume = volume, buildSettings = settings, rebuildMesh](const CancellationToken& cancellation)
    {
      return BuildSHGlyphField(*buildVolume, buildSettings, rebuildMesh, cancellation);
    });
}

void SHGlyphField::InstallBuildResult(SHGlyphBuildResult result)
{
  if (result.geometry)
  {
    geometry = std::move(result.geometry);
    basisTexture = std::make_unique<BufferTexture>(TextureFormat::RGBA32F, std::move(result.basisTable));
  }
  instanceBuffer = std::move(result.instances);
  glyphCount = result.glyphCount;
}
