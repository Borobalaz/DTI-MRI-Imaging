#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "scene/geometry/IcosphereGeometry.h"
#include "scene/glyphs/SHVolume.h"
#include "scene/object/IDrawable.h"
#include "scene/object/IUpdateable.h"
#include "inspection/InspectField.h"
#include "render/geometry/InstanceBuffer.h"
#include "render/Material.h"
#include "render/Shader.h"
#include "render/texture/BufferTexture.h"
#include "render/uniform/UniformProvider.h"
#include "scene/extraction/RenderProxy.h"
#include "foundation/threading/BackgroundTask.h"
#include "foundation/threading/Cancellation.h"
#include "scene/object/Transform.h"
#include "scene/slicing/SlicePlane.h"

// Volume: one glyph per stride-th voxel over the whole dataset. Slice: glyphs resampled on the
// scene's slice plane at stride 1 (stride is ignored).
enum class SHGlyphDisplayMode { Volume, Slice };

// Inputs of one glyph-field build; any change triggers a (latest-wins) rebuild.
struct SHGlyphBuildSettings
{
  int stride = 3;                    // voxel step along every axis, >= 1 (Volume mode only)
  float maskThreshold = 0.3f;        // voxels whose mask value is below this get no glyph
  unsigned int subdivisionLevel = 3; // IcosphereGeometry level of the glyph mesh
  SHGlyphDisplayMode displayMode = SHGlyphDisplayMode::Volume;
};

// CPU-only output of BuildSHGlyphField, moved into an SHGlyphField on the engine thread.
struct SHGlyphBuildResult
{
  // Transforms (locations 4-7), aInstanceMaxRadius (location 8) and instanceCoefficients
  // (RGBA32F, kCoefficientFloatsPerInstance floats per instance), all declared and filled.
  InstanceBuffer instances;
  size_t glyphCount = 0;

  // Set only when the build was asked to (re)build the mesh: the new glyph mesh and its basis
  // table (RealSH::BuildBasisTable over its directions, 12 texels per vertex).
  std::shared_ptr<IcosphereGeometry> geometry;
  std::vector<float> basisTable;
};

// Builds the instance data of a glyph field. Volume mode: one glyph per stride-th voxel that
// passes the mask and has a non-zero coefficient vector, placed like Volume's
// unit-cube-times-extents mapping; polls `cancellation` once per z-slice. Slice mode: glyphs on
// a regular grid (spacing = smallest voxel edge) on `slicePlane`, coefficients and mask
// trilinearly interpolated; polls once per grid row. `slicePlane` is ignored in Volume mode.
// Both modes produce the same SHGlyphBuildResult layout. Graphics-API free, meant for a worker
// thread; throws OperationCancelled on cancellation and std::runtime_error for a malformed volume.
SHGlyphBuildResult BuildSHGlyphField(const SHVolume& volume,
                                     const SHGlyphBuildSettings& settings,
                                     const SlicePlane& slicePlane,
                                     bool rebuildMesh,
                                     const CancellationToken& cancellation);

// Instanced field of spherical-harmonic glyphs over an SHVolume, drawn through the generic
// instanced mesh pass with the glyph shader (sh_glyph_vertex/fragment.glsl). Instance data is
// built on a BackgroundTask and swapped in by Update() on the engine thread; until the first
// build lands the field draws nothing.
class SHGlyphField : public UniformProvider, public IDrawable, public IUpdateable, public InspectProvider
{
public:
  // Payload contract shared with sh_glyph_vertex.glsl. kCoefficientTexelsPerInstance must equal
  // RealSH::kBasisTexelsPerDirection: the shader dots texel t of both.
  static constexpr size_t kCoefficientFloatsPerInstance = 48; // 45 coefficients, zero-padded
  static constexpr int kCoefficientTexelsPerInstance = 12;    // RGBA32F texels
  static constexpr unsigned int kMaxRadiusLocation = 8;
  // samplerBuffer shGlyph.basis: RealSH::kBasisTexelsPerDirection (12) RGBA32F texels per mesh
  // vertex, texel t = (Y_4t .. Y_4t+3).
  static constexpr unsigned int kBasisTextureUnit = 12;

  SHGlyphField(const std::string& id,
               std::shared_ptr<const SHVolume> volume,
               std::shared_ptr<Shader> glyphShader,
               SHGlyphBuildSettings settings = {});

  // UniformProvider / IDrawable / IUpdateable
  void Apply(IUniformSink& shader) const override;
  void BuildRenderProxy(RenderProxy& renderProxy) const override;
  void Update(float deltaTime) override;

  const std::string& GetId() const { return id; }
  const glm::vec3& GetRotation() const { return transform.GetRotation(); }
  void SetRotation(const glm::vec3& newRotation) { transform.SetRotation(newRotation); }

  // Each change requests a rebuild: a running build is cancelled and a new one starts in
  // Update() once it has stopped, so rapid edits are latest-wins and never block.
  void SetStride(int newStride);
  void SetMaskThreshold(float newThreshold);
  void SetSubdivisionLevel(unsigned int newLevel);
  // Swaps the SH source (latest-wins rebuild of the instance data; the mesh and basis table are
  // kept, since they depend only on the subdivision level). Null is ignored with a warning.
  void SetVolume(std::shared_ptr<const SHVolume> newVolume);

  // Shared scene plane, read only on the engine thread (Update/StartBuild). In Slice mode a
  // version change requests a latest-wins rebuild; the build gets a value snapshot. Null means
  // the default axial plane through the centre.
  void SetSlicePlane(std::shared_ptr<const SlicePlane> newPlane);
  void SetDisplayMode(SHGlyphDisplayMode newMode);
  SHGlyphDisplayMode GetDisplayMode() const { return settings.displayMode; }

  // InspectProvider implementation
  bool HasVisibility() const override { return true; }
  bool IsVisible() const override { return visible; }
  void SetVisible(bool newVisible) override { visible = newVisible; }
  std::optional<float> CastRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const override;
  std::string GetInspectDisplayName() const override { return id; }
  std::vector<InspectFieldPtr> GetInspectFields() override;

private:
  void RequestRebuild();
  void StartBuild();
  void InstallBuildResult(SHGlyphBuildResult result);

  const std::string id;
  bool visible = true;
  Transform transform;

  std::shared_ptr<const SHVolume> volume;
  std::shared_ptr<Material> material;
  SHGlyphBuildSettings settings;
  float glyphScale = 0.9f;
  bool normalizeGlyphs = true;

  std::shared_ptr<IcosphereGeometry> geometry;
  std::unique_ptr<BufferTexture> basisTexture;
  InstanceBuffer instanceBuffer;
  size_t glyphCount = 0;

  std::unique_ptr<BackgroundTask<SHGlyphBuildResult>> buildTask;
  bool rebuildPending = false;

  std::shared_ptr<const SlicePlane> slicePlane;
  std::uint64_t buildPlaneVersion = 0; // plane version captured by the most recent StartBuild
};
