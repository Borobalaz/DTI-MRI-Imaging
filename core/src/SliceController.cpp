#include "SliceController.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

#include "foundation/log/EngineLog.h"
#include "render/core/RenderCommands.h"
#include "render/Material.h"
#include "render/Shader.h"
#include "scene/extraction/RenderProxy.h"
#include "scene/geometry/ConvexPolygonFillGeometry.h"
#include "scene/geometry/LineLoopGeometry.h"

namespace
{
  constexpr const char* kLogSource = "SliceController";
  constexpr const char* kPlaneGroup = "Slice Plane";
  constexpr const char* kFillGroup = "Fill";
  const glm::vec3 kOutlineColor(1.0f, 0.85f, 0.0f);

  // Preset normals in dataset object space.
  const glm::vec3 kAxialNormal(0.0f, 0.0f, 1.0f);
  const glm::vec3 kCoronalNormal(0.0f, 1.0f, 0.0f);
  const glm::vec3 kSagittalNormal(1.0f, 0.0f, 0.0f);
}

SliceController::SliceController()
  : plane(std::make_shared<SlicePlane>()),
    outlineGeometry(std::make_shared<LineLoopGeometry>()),
    fillGeometry(std::make_shared<ConvexPolygonFillGeometry>())
{
}

SliceController::~SliceController() = default;

void SliceController::SetDatasetGeometry(const glm::vec3& objectExtents, float voxelStep)
{
  // A box change bumps the plane version, so the next Update rebuilds the geometry.
  plane->SetBoxHalfExtents(objectExtents * 0.5f);
  if (std::isfinite(voxelStep) && voxelStep > 0.0f)
  {
    offsetStep = voxelStep;
  }
  hasDatasetGeometry = true;
}

void SliceController::SetShader(std::shared_ptr<Shader> unlitShader)
{
  if (unlitShader == shader && outlineMaterial)
  {
    return;
  }
  shader = std::move(unlitShader);

  if (!shader)
  {
    outlineMaterial.reset();
    fillMaterial.reset();
    return;
  }

  outlineMaterial = std::make_shared<Material>(shader);
  outlineMaterial->SetBaseColor(kOutlineColor);

  fillMaterial = std::make_shared<Material>(shader);
  fillMaterial->SetBaseColor(fillColor);
  fillMaterial->SetOpacity(fillOpacity);
}

float SliceController::GetOffsetLimit() const
{
  return plane->GetOffsetLimit();
}

void SliceController::Update(float deltaTime)
{
  if (!builtPlaneVersion || *builtPlaneVersion != plane->GetVersion())
  {
    RebuildGeometry();
  }
  IUpdateable::Update(deltaTime);
}

void SliceController::RebuildGeometry()
{
  // The fill is rebuilt even while hidden, so toggling it on shows the current plane at once.
  const std::vector<glm::vec3> polygon = plane->IntersectBox(plane->GetBoxHalfExtents());
  outlineGeometry->SetLoop(polygon, plane->GetNormal());
  fillGeometry->SetPolygon(polygon, plane->GetNormal());
  builtPlaneVersion = plane->GetVersion();
}

void SliceController::BuildRenderProxy(RenderProxy& renderProxy) const
{
  // Before the first dataset there is no box to slice; an empty polygon has nothing to draw.
  const bool hasDrawableShader = shader && shader->IsValid() && outlineMaterial && fillMaterial;
  if (!visible || !hasDatasetGeometry || !hasDrawableShader || outlineGeometry->GetIndices().empty())
  {
    renderProxy.visible = false;
    return;
  }

  const glm::mat4 modelMatrix = transform.GetModelMatrix();

  MeshRenderCommand outline;
  outline.geometry = outlineGeometry;
  outline.material = outlineMaterial;
  outline.modelMatrix = modelMatrix;
  outline.frameUniforms = renderProxy.frameUniforms;
  MeshRenderBatch outlineBatch;
  outlineBatch.draws.push_back(std::move(outline));
  renderProxy.data.Append(std::move(outlineBatch));

  if (fillEnabled && fillOpacity > 0.0f)
  {
    TransparentMeshRenderCommand fill;
    fill.geometry = fillGeometry;
    fill.material = fillMaterial;
    fill.modelMatrix = modelMatrix;
    fill.frameUniforms = renderProxy.frameUniforms;
    TransparentMeshRenderBatch fillBatch;
    fillBatch.draws.push_back(std::move(fill));
    renderProxy.data.Append(std::move(fillBatch));
  }

  renderProxy.visible = true;
}

std::string SliceController::GetInspectDisplayName() const
{
  return "Slice Plane";
}

const char* SliceController::PresetLabel(Preset value)
{
  switch (value)
  {
    case Preset::Axial: return "Axial";
    case Preset::Coronal: return "Coronal";
    case Preset::Sagittal: return "Sagittal";
    case Preset::Custom: return "Custom";
  }
  return "Custom";
}

std::optional<SliceController::Preset> SliceController::PresetFromLabel(const std::string& label)
{
  for (Preset candidate : {Preset::Axial, Preset::Coronal, Preset::Sagittal, Preset::Custom})
  {
    if (label == PresetLabel(candidate))
    {
      return candidate;
    }
  }
  return std::nullopt;
}

void SliceController::ApplyPreset(Preset newPreset)
{
  glm::vec3 normal;
  switch (newPreset)
  {
    case Preset::Axial: normal = kAxialNormal; break;
    case Preset::Coronal: normal = kCoronalNormal; break;
    case Preset::Sagittal: normal = kSagittalNormal; break;
    case Preset::Custom: return; // "Custom" only reflects a hand-edited normal
  }

  plane->SetNormal(normal);
  plane->SetOffset(0.0f);
  preset = newPreset;
}

std::vector<InspectFieldPtr> SliceController::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields;

  auto presetField = MakeInspectField("slice.preset", "Preset", kPlaneGroup, InspectFieldType::Dropdown,
    std::string(PresetLabel(preset)),
    [this]() -> InspectValue { return std::string(PresetLabel(preset)); },
    [this](const InspectValue& value)
  {
    const auto* label = std::get_if<std::string>(&value);
    if (!label)
    {
      return;
    }
    const std::optional<Preset> selected = PresetFromLabel(*label);
    if (selected && *selected != Preset::Custom)
    {
      ApplyPreset(*selected);
    }
  });
  presetField->enumOptions = {"Axial", "Coronal", "Sagittal", "Custom"};
  fields.push_back(presetField);

  auto normalField = MakeInspectField("slice.normal", "Normal", kPlaneGroup, InspectFieldType::Vector3,
    plane->GetNormal(),
    [this]() -> InspectValue { return plane->GetNormal(); },
    [this](const InspectValue& value)
  {
    const auto* normal = std::get_if<glm::vec3>(&value);
    if (!normal)
    {
      return;
    }
    const std::uint64_t versionBefore = plane->GetVersion();
    if (!plane->SetNormal(*normal))
    {
      EngineLog::Warning("Slice plane normal must be finite and non-zero; edit ignored", kLogSource);
      return;
    }
    if (plane->GetVersion() != versionBefore)
    {
      preset = Preset::Custom;
    }
  });
  fields.push_back(normalField);

  // The range is frozen at fetch time; DtiVolumeScene re-fetches when the limit changes, and the
  // setter clamps anyway, so a stale UI range is harmless.
  auto offsetField = MakeInspectField("slice.offset", "Offset", kPlaneGroup, InspectFieldType::Number,
    static_cast<double>(plane->GetOffset()),
    [this]() -> InspectValue { return static_cast<double>(plane->GetOffset()); },
    [this](const InspectValue& value)
  {
    if (const auto* number = std::get_if<double>(&value))
    {
      plane->SetOffset(static_cast<float>(*number));
    }
  });
  offsetField->minimum = -static_cast<double>(GetOffsetLimit());
  offsetField->maximum = static_cast<double>(GetOffsetLimit());
  offsetField->step = static_cast<double>(offsetStep);
  fields.push_back(offsetField);

  auto fillEnabledField = MakeInspectField("slice.fillEnabled", "Fill", kFillGroup, InspectFieldType::Boolean,
    fillEnabled,
    [this]() -> InspectValue { return fillEnabled; },
    [this](const InspectValue& value)
  {
    if (const auto* enabled = std::get_if<bool>(&value))
    {
      fillEnabled = *enabled;
    }
  });
  fields.push_back(fillEnabledField);

  auto fillColorField = MakeInspectField("slice.fillColor", "Fill Colour", kFillGroup, InspectFieldType::Color,
    fillColor,
    [this]() -> InspectValue { return fillColor; },
    [this](const InspectValue& value)
  {
    const auto* color = std::get_if<glm::vec3>(&value);
    if (!color)
    {
      return;
    }
    fillColor = *color;
    if (fillMaterial)
    {
      fillMaterial->SetBaseColor(fillColor);
    }
  });
  fields.push_back(fillColorField);

  auto fillOpacityField = MakeInspectField("slice.fillOpacity", "Fill Opacity", kFillGroup, InspectFieldType::Number,
    static_cast<double>(fillOpacity),
    [this]() -> InspectValue { return static_cast<double>(fillOpacity); },
    [this](const InspectValue& value)
  {
    const auto* number = std::get_if<double>(&value);
    if (!number || std::isnan(*number))
    {
      return;
    }
    fillOpacity = std::clamp(static_cast<float>(*number), 0.0f, 1.0f);
    if (fillMaterial)
    {
      fillMaterial->SetOpacity(fillOpacity);
    }
  });
  fillOpacityField->minimum = 0.0;
  fillOpacityField->maximum = 1.0;
  fillOpacityField->step = 0.05;
  fields.push_back(fillOpacityField);

  return fields;
}
