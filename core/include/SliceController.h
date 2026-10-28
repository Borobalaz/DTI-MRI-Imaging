#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "inspection/InspectField.h"
#include "scene/object/IDrawable.h"
#include "scene/object/IUpdateable.h"
#include "scene/object/Transform.h"
#include "scene/slicing/SlicePlane.h"

class ConvexPolygonFillGeometry;
class LineLoopGeometry;
class Material;
class Shader;
struct RenderProxy;

// Owns the scene's single slice plane (dataset object space, see SlicePlane.h), shows it in the
// object list as "Slice Plane" (Preset / Normal / Offset / Fill controls), and draws it: an
// always-on outline (plane intersect dataset box, line loop, opaque mesh pass) and an optional
// semi-transparent fill (TransparentPass). Both use the unlit shader; colour and opacity travel
// through their Materials (material.albedoFactor / material.opacity).
//
// The transform carries only the dataset rotation (kProcessedObjectRotation + spin, driven by
// DtiVolumeScene); scale stays 1 because the geometry is already in dataset space.
// Geometry is rebuilt in Update() when the plane version changes (engine/GL thread; lazy re-upload).
//
// Thread contract: externally synchronised by the scene lock (field getters/setters arrive
// through LockedInspectionService; Update/BuildRenderProxy run in the engine tick).
class SliceController final : public InspectProvider, public IDrawable, public IUpdateable
{
public:
  SliceController();
  ~SliceController() override; // out of line: geometry types are forward-declared

  std::shared_ptr<const SlicePlane> GetPlane() const { return plane; }

  // Per applied dataset: re-clamps the offset to the new box (normal/offset/preset are kept),
  // sets the Offset field step to one voxel, and enables drawing.
  void SetDatasetGeometry(const glm::vec3& objectExtents, float voxelStep);

  // GL-thread compiled unlit shader (DtiVolumeScene's cache). Rebuilds both materials when the
  // shader object changes; null (or a shader that failed to build) means nothing is drawn.
  void SetShader(std::shared_ptr<Shader> unlitShader);

  const glm::vec3& GetRotation() const { return transform.GetRotation(); }
  void SetRotation(const glm::vec3& newRotation) { transform.SetRotation(newRotation); }

  // Inspector-metadata inputs; DtiVolumeScene re-fetches fields when they change.
  float GetOffsetLimit() const;
  float GetOffsetStep() const { return offsetStep; }

  // IDrawable / IUpdateable
  void BuildRenderProxy(RenderProxy& renderProxy) const override;
  void Update(float deltaTime) override;

  // InspectProvider (no visibility toggle: the outline is always on, the fill has its own checkbox)
  std::string GetInspectDisplayName() const override;
  std::vector<InspectFieldPtr> GetInspectFields() override;

private:
  enum class Preset { Axial, Coronal, Sagittal, Custom };

  void ApplyPreset(Preset newPreset);
  static const char* PresetLabel(Preset value);
  static std::optional<Preset> PresetFromLabel(const std::string& label);
  void RebuildGeometry();

  std::shared_ptr<SlicePlane> plane;
  Preset preset = Preset::Axial;  // matches SlicePlane's default normal (0, 0, 1)
  float offsetStep = 0.01f;       // replaced by the dataset's voxel step on the first apply
  bool hasDatasetGeometry = false;
  Transform transform;

  std::shared_ptr<LineLoopGeometry> outlineGeometry;
  std::shared_ptr<ConvexPolygonFillGeometry> fillGeometry;
  std::shared_ptr<Shader> shader;
  std::shared_ptr<Material> outlineMaterial; // opacity stays 1
  std::shared_ptr<Material> fillMaterial;
  std::optional<std::uint64_t> builtPlaneVersion; // plane version the geometries reflect

  bool fillEnabled = false;
  glm::vec3 fillColor{1.0f, 0.85f, 0.0f};
  float fillOpacity = 0.25f; // clamped to [0, 1]; mirrored into fillMaterial
};
