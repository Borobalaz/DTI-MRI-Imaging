#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

// Dataset object space
// --------------------
// The space SHGlyphField places glyphs in and Volume's transform scale maps its unit cube into:
// centred at the origin, box = [-E/2, +E/2] with E = DatasetObjectExtents(dimensions, spacing)
// = dimensions * spacing / max component, so the longest axis has length 1 ("normalised object
// units"). Voxel (i,j,k)'s centre is ((ijk + 0.5) / dimensions - 0.5) * E. Volume's unit cube
// q in [-0.5,0.5]^3 maps here by p = E * q (componentwise). Everything processed by
// DtiVolumeScene shares this space (up to the common object rotation), so a plane stored here is
// fixed to the data and rotates with it.

// Box size of a dims x spacing grid in dataset object space (see above). Non-positive input
// components are treated as 1e-6 so the result is always finite and positive.
glm::vec3 DatasetObjectExtents(const glm::ivec3& dimensions, const glm::vec3& spacing);

// Size of the smallest voxel edge in dataset object space (equals SHGlyphField's stride-1 cellSize).
float DatasetVoxelStep(const glm::ivec3& dimensions, const glm::vec3& spacing);

// A plane dot(normal, p) == offset in some object space.
struct PlaneEquation
{
  glm::vec3 normal{0.0f, 0.0f, 1.0f}; // unit length
  float offset = 0.0f;
};

// Orthonormal in-plane basis: (u, v, n) is right-handed (cross(u, v) == n).
struct PlaneBasis
{
  glm::vec3 u{1.0f, 0.0f, 0.0f};
  glm::vec3 v{0.0f, 1.0f, 0.0f};
};

// One slicing plane in dataset object space: { p : dot(normal, p) == offset }, with `normal`
// unit length and `offset` the signed distance of the plane from the dataset centre along
// `normal`, in normalised object units. The plane also stores the dataset box half-extents so
// SetOffset can clamp to the box; they default to the unit cube (0.5, 0.5, 0.5) until the owner
// sets the real dataset box.
//
// A plain CPU value type with no graphics API: copying it yields a consistent snapshot
// (including the version), which is how background builds capture it. Not internally
// synchronised: the shared instance owned by SliceController is only read and written with the
// scene lock held (inspector setters via LockedInspectionService, engine-thread Update/Apply).
class SlicePlane
{
public:
  SlicePlane() = default;

  const glm::vec3& GetNormal() const { return normal; }
  float GetOffset() const { return offset; }
  const glm::vec3& GetBoxHalfExtents() const { return boxHalfExtents; }
  // Bumped once by every setter call that changed normal, offset or box half-extents.
  std::uint64_t GetVersion() const { return version; }

  // Normalises `newNormal` and re-clamps the offset to the box. Returns false (state unchanged)
  // when the input is non-finite or shorter than 1e-6.
  bool SetNormal(const glm::vec3& newNormal);

  // Clamps to [-GetOffsetLimit(), +GetOffsetLimit()]. Non-finite input is ignored.
  void SetOffset(float newOffset);

  // Sets the dataset box [-h, +h] the offset is clamped against and re-clamps the offset.
  // Negative/non-finite components are treated as 0.
  void SetBoxHalfExtents(const glm::vec3& halfExtents);

  // Half-extent of the stored box along the normal: dot(abs(normal), boxHalfExtents).
  float GetOffsetLimit() const;

  // dot(normal, p) - offset; positive on the side the normal points to.
  float SignedDistance(const glm::vec3& p) const;

  // In-plane basis; the helper axis is the coordinate axis least aligned with the normal.
  PlaneBasis GetInPlaneBasis() const;

  // Plane intersected with the box [-halfExtents, +halfExtents]: the vertices of the convex
  // intersection polygon in the same space, ordered counter-clockwise seen from +normal, first
  // vertex not repeated. Empty when the intersection has fewer than 3 distinct points (no
  // intersection, or touching only a corner or an edge).
  std::vector<glm::vec3> IntersectBox(const glm::vec3& halfExtents) const;

  // This plane re-expressed in the unit-cube object space of a Volume whose transform scale is
  // `objectExtents` (p = objectExtents * q): normal' = normalize(objectExtents * normal),
  // offset' = offset / length(objectExtents * normal).
  PlaneEquation ToUnitCubeSpace(const glm::vec3& objectExtents) const;

private:
  glm::vec3 normal{0.0f, 0.0f, 1.0f};
  float offset = 0.0f;
  glm::vec3 boxHalfExtents{0.5f, 0.5f, 0.5f};
  std::uint64_t version = 0;
};
