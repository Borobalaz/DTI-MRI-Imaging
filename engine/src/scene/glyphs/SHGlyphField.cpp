#include "scene/glyphs/SHGlyphField.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

#include "foundation/log/EngineLog.h"
#include "scene/spherical_harmonics/RealSH.h"

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

  // Absorbs float error for grid points exactly on the outermost voxel-centre layer (voxel units).
  constexpr float kVoxelEpsilon = 1e-4f;

  // Per-instance streams of one build, appended identically by the volume and slice paths.
  struct GlyphInstanceAccumulator
  {
    std::vector<glm::mat4> transforms;
    std::vector<float> maxRadii;
    std::vector<float> paddedCoefficients;

    void Append(const glm::vec3& center, float radius, const float* coefficients,
                size_t coefficientCount, const std::vector<double>& directionBasis)
    {
      transforms.push_back(glm::scale(glm::translate(glm::mat4(1.0f), center), glm::vec3(radius)));
      maxRadii.push_back(MaxPositiveRadius(coefficients, coefficientCount, directionBasis));
      paddedCoefficients.insert(paddedCoefficients.end(), coefficients, coefficients + coefficientCount);
      paddedCoefficients.insert(paddedCoefficients.end(),
                                SHGlyphField::kCoefficientFloatsPerInstance - coefficientCount, 0.0f);
    }
  };

  // Trilinear sample of the coefficients (and the mask, if any) at continuous voxel coordinate
  // `voxel` (voxel centres at integers, already clamped to [0, dims - 1]). Equals GL_LINEAR
  // sampling of a 3D texture at (voxel + 0.5) / dims, i.e. what the volume slice shader sees.
  void SampleTrilinear(const SHVolume& volume, const glm::vec3& voxel, size_t coefficientCount,
                       std::array<float, RealSH::kSHMaxCoefficients>& coefficients, float& maskValue)
  {
    const glm::ivec3& dimensions = volume.dimensions;
    glm::ivec3 lower(0);
    glm::ivec3 upper(0);
    glm::vec3 fraction(0.0f);
    for (int axis = 0; axis < 3; ++axis)
    {
      lower[axis] = std::clamp(static_cast<int>(std::floor(voxel[axis])), 0, std::max(dimensions[axis] - 2, 0));
      upper[axis] = std::min(lower[axis] + 1, dimensions[axis] - 1);
      fraction[axis] = dimensions[axis] == 1 ? 0.0f : voxel[axis] - static_cast<float>(lower[axis]);
    }

    coefficients.fill(0.0f);
    maskValue = 0.0f;
    const std::vector<float>* maskVoxels = volume.mask ? &volume.mask->GetVoxels() : nullptr;
    for (int corner = 0; corner < 8; ++corner)
    {
      const bool useUpperX = (corner & 1) != 0;
      const bool useUpperY = (corner & 2) != 0;
      const bool useUpperZ = (corner & 4) != 0;
      const int x = useUpperX ? upper.x : lower.x;
      const int y = useUpperY ? upper.y : lower.y;
      const int z = useUpperZ ? upper.z : lower.z;
      const float weight = (useUpperX ? fraction.x : 1.0f - fraction.x) *
                           (useUpperY ? fraction.y : 1.0f - fraction.y) *
                           (useUpperZ ? fraction.z : 1.0f - fraction.z);

      const float* voxelCoefficients = volume.VoxelCoefficients(x, y, z);
      for (size_t k = 0; k < coefficientCount; ++k)
      {
        coefficients[k] += weight * voxelCoefficients[k];
      }
      if (maskVoxels)
      {
        maskValue += weight * (*maskVoxels)[VolumeData::FlatIndex(x, y, z, dimensions.x, dimensions.y)];
      }
    }
  }

  // Slice display: glyphs on a regular grid on `plane` (spacing cellSize = stride 1), radius
  // 0.5 * cellSize, coefficients and mask trilinearly interpolated. Polls cancellation once per
  // grid row. Appends nothing when the plane misses the box.
  void AppendSliceGlyphs(const SHVolume& volume,
                         const SHGlyphBuildSettings& settings,
                         const SlicePlane& plane,
                         const glm::vec3& objectExtents,
                         float cellSize,
                         size_t coefficientCount,
                         const std::vector<double>& directionBasis,
                         const CancellationToken& cancellation,
                         GlyphInstanceAccumulator& glyphs)
  {
    if (!(cellSize > 0.0f))
    {
      return;
    }

    const std::vector<glm::vec3> polygon = plane.IntersectBox(0.5f * objectExtents);
    if (polygon.size() < 3)
    {
      return;
    }

    const glm::vec3& normal = plane.GetNormal();
    const PlaneBasis basis = plane.GetInPlaneBasis();
    const glm::ivec3& dimensions = volume.dimensions;
    const glm::vec3 dimensionsF(dimensions);

    // Anchor the grid at the projection of a voxel centre: voxel-aligned for axis-aligned planes
    // and stable in-plane while the offset is scrubbed.
    const glm::ivec3 centreVoxel = dimensions / 2;
    const glm::vec3 centreVoxelPosition = ((glm::vec3(centreVoxel) + 0.5f) / dimensionsF - 0.5f) * objectExtents;
    const glm::vec3 anchor = centreVoxelPosition - (glm::dot(normal, centreVoxelPosition) - plane.GetOffset()) * normal;

    float sMin = std::numeric_limits<float>::max();
    float sMax = std::numeric_limits<float>::lowest();
    float tMin = std::numeric_limits<float>::max();
    float tMax = std::numeric_limits<float>::lowest();
    for (const glm::vec3& vertex : polygon)
    {
      const float s = glm::dot(vertex - anchor, basis.u);
      const float t = glm::dot(vertex - anchor, basis.v);
      sMin = std::min(sMin, s);
      sMax = std::max(sMax, s);
      tMin = std::min(tMin, t);
      tMax = std::max(tMax, t);
    }
    const int iMin = static_cast<int>(std::ceil(sMin / cellSize));
    const int iMax = static_cast<int>(std::floor(sMax / cellSize));
    const int jMin = static_cast<int>(std::ceil(tMin / cellSize));
    const int jMax = static_cast<int>(std::floor(tMax / cellSize));

    const glm::vec3 maxVoxel = dimensionsF - 1.0f;
    const float glyphRadius = 0.5f * cellSize;
    std::array<float, RealSH::kSHMaxCoefficients> coefficients{};
    float maskValue = 0.0f;
    for (int j = jMin; j <= jMax; ++j)
    {
      cancellation.ThrowIfCancellationRequested();
      for (int i = iMin; i <= iMax; ++i)
      {
        const glm::vec3 position = anchor + (static_cast<float>(i) * cellSize) * basis.u
                                          + (static_cast<float>(j) * cellSize) * basis.v;
        // Every point inside the voxel-centre box is inside the dataset box, hence inside the
        // polygon; the polygon only bounds the loop.
        glm::vec3 voxel = (position / objectExtents + 0.5f) * dimensionsF - 0.5f;
        if (glm::any(glm::lessThan(voxel, glm::vec3(-kVoxelEpsilon))) ||
            glm::any(glm::greaterThan(voxel, maxVoxel + kVoxelEpsilon)))
        {
          continue;
        }
        voxel = glm::clamp(voxel, glm::vec3(0.0f), maxVoxel);

        SampleTrilinear(volume, voxel, coefficientCount, coefficients, maskValue);
        if (volume.mask && maskValue < settings.maskThreshold)
        {
          continue;
        }
        if (!HasNonZeroCoefficient(coefficients.data(), coefficientCount))
        {
          continue;
        }

        glyphs.Append(position, glyphRadius, coefficients.data(), coefficientCount, directionBasis);
      }
    }
  }
}

static_assert(SHGlyphField::kCoefficientTexelsPerInstance == RealSH::kBasisTexelsPerDirection,
              "the glyph shader dots coefficient texel t with basis texel t");

SHGlyphBuildResult BuildSHGlyphField(const SHVolume& volume,
                                     const SHGlyphBuildSettings& settings,
                                     const SlicePlane& slicePlane,
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

  GlyphInstanceAccumulator glyphs;
  if (settings.displayMode == SHGlyphDisplayMode::Slice)
  {
    AppendSliceGlyphs(volume, settings, slicePlane, objectExtents, cellSize, coefficientCount,
                      directionBasis, cancellation, glyphs);
  }
  else
  {
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
          glyphs.Append(center, glyphRadius, coefficients, coefficientCount, directionBasis);
        }
      }
    }
  }

  SHGlyphBuildResult result;
  result.glyphCount = glyphs.transforms.size();
  result.instances.SetTransforms(std::move(glyphs.transforms));
  const InstanceStreamId maxRadiusStream =
    result.instances.AddAttributeStream({SHGlyphField::kMaxRadiusLocation, 1});
  const InstanceStreamId coefficientStream = result.instances.AddBufferStream(
    {"instanceCoefficients", TextureFormat::RGBA32F, SHGlyphField::kCoefficientFloatsPerInstance});
  result.instances.SetStreamData(maxRadiusStream, std::move(glyphs.maxRadii));
  result.instances.SetStreamData(coefficientStream, std::move(glyphs.paddedCoefficients));

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
  material->SetCullBackFaces(true);

  if (!this->volume)
  {
    EngineLog::Error("SH glyph field '" + id + "' has no SH volume; it will draw nothing", kLogSource);
    return;
  }
  // The first build starts in Update(), so the owner can hand over the slice plane (and display
  // mode) before any build captures its inputs.
  rebuildPending = true;
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

  // Slice mode follows the shared plane: any change since the last StartBuild requests a
  // latest-wins rebuild. Comparing against the *started* version (not the installed one) means a
  // build already running for the current version is not re-requested every frame.
  if (settings.displayMode == SHGlyphDisplayMode::Slice && slicePlane && !rebuildPending &&
      slicePlane->GetVersion() != buildPlaneVersion)
  {
    RequestRebuild();
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
  // Stride is ignored in Slice mode; SetDisplayMode requests the rebuild when switching back.
  if (settings.displayMode == SHGlyphDisplayMode::Volume)
  {
    RequestRebuild();
  }
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

void SHGlyphField::SetVolume(std::shared_ptr<const SHVolume> newVolume)
{
  if (!newVolume)
  {
    EngineLog::Warning("SH glyph field '" + id + "': SetVolume(nullptr) ignored", kLogSource);
    return;
  }
  if (newVolume == volume)
  {
    return;
  }
  volume = std::move(newVolume);
  RequestRebuild();
}

void SHGlyphField::SetSlicePlane(std::shared_ptr<const SlicePlane> newPlane)
{
  if (newPlane == slicePlane)
  {
    return;
  }
  slicePlane = std::move(newPlane);
  if (settings.displayMode == SHGlyphDisplayMode::Slice)
  {
    RequestRebuild();
  }
}

void SHGlyphField::SetDisplayMode(SHGlyphDisplayMode newMode)
{
  if (newMode == settings.displayMode)
  {
    return;
  }
  settings.displayMode = newMode;
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

  auto displayModeLabel = [this]() -> std::string
  {
    return settings.displayMode == SHGlyphDisplayMode::Slice ? "Slice" : "Volume";
  };
  auto displayModeField = MakeInspectField("displayMode", "Display", "Glyphs", InspectFieldType::Dropdown, displayModeLabel(),
    [displayModeLabel]() -> InspectValue { return displayModeLabel(); },
    [this](const InspectValue &value)
  {
    const auto *label = std::get_if<std::string>(&value);
    if (!label)
    {
      return;
    }
    if (*label == "Slice")
    {
      SetDisplayMode(SHGlyphDisplayMode::Slice);
    }
    else if (*label == "Volume")
    {
      SetDisplayMode(SHGlyphDisplayMode::Volume);
    }
  });
  displayModeField->enumOptions = {"Volume", "Slice"};
  fields.push_back(displayModeField);

  // Slice mode always samples at stride 1, so the greyed-out field shows the effective stride.
  auto effectiveStride = [this]() -> double
  {
    return settings.displayMode == SHGlyphDisplayMode::Slice ? 1.0 : static_cast<double>(settings.stride);
  };
  auto strideField = MakeInspectField("stride", "Stride", "Glyphs", InspectFieldType::Number, effectiveStride(),
    [effectiveStride]() -> InspectValue { return effectiveStride(); },
    [this](const InspectValue &value)
  {
    if (const auto *number = std::get_if<double>(&value)) SetStride(static_cast<int>(*number));
  });
  strideField->minimum = 1.0;
  strideField->maximum = 32.0;
  strideField->step = 1.0;
  strideField->readOnly = settings.displayMode == SHGlyphDisplayMode::Slice;
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

  // Value snapshot: the worker never reads the shared plane.
  const SlicePlane planeSnapshot = slicePlane ? *slicePlane : SlicePlane{};
  buildPlaneVersion = planeSnapshot.GetVersion();

  // The task copies the shared_ptr, the settings and the plane, so later edits never race the worker.
  buildTask = std::make_unique<BackgroundTask<SHGlyphBuildResult>>(
    [buildVolume = volume, buildSettings = settings, planeSnapshot, rebuildMesh](const CancellationToken& cancellation)
    {
      return BuildSHGlyphField(*buildVolume, buildSettings, planeSnapshot, rebuildMesh, cancellation);
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
