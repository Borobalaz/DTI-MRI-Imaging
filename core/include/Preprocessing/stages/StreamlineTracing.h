#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

class Mesh;

// Model-independent helpers shared by the streamline trackers (DtiFiberTractographyStage,
// FodfFiberTractographyStage). Positions are in voxel (index) coordinates unless stated otherwise.
namespace StreamlineTracing
{
  // Trilinear sample of a scalar volume; the point is clamped into the volume and a non-finite
  // result is returned as 0.
  float SampleTrilinear(const std::vector<float> &voxels,
                        const glm::vec3 &point,
                        const glm::ivec3 &dims);

  // Whether a voxel-space point lies inside [0, dims - 1] on every axis.
  bool IsInBounds(const glm::vec3 &point, const glm::ivec3 &dims);

  // Voxel coordinates -> the normalized, origin-centred object space shared by the scene's
  // streamline, seed and surface meshes (physical extent divided by its largest axis).
  glm::vec3 VoxelToObjectSpace(const glm::vec3 &voxelPosition,
                               const glm::ivec3 &dims,
                               const glm::vec3 &spacing);

  // Unit physical-space direction -> unit voxel-space step direction (divided by the spacing and
  // renormalized), or the zero vector when degenerate.
  glm::vec3 ToIndexSpaceStepDirection(const glm::vec3 &unitDirection, const glm::vec3 &spacing);

  // Joins two halves traced from the same seed in opposite directions, both starting at the seed:
  // the backward half reversed, then the forward half, with the duplicated seed point dropped.
  std::vector<glm::vec3> JoinBidirectional(const std::vector<glm::vec3> &backward,
                                           const std::vector<glm::vec3> &forward);

  // One tube mesh drawing every streamline (object-space polylines) in a single primitive-restart
  // draw. Returns nullptr when there are no points. No material is assigned: the scene owns render
  // policy.
  std::shared_ptr<Mesh> BuildStreamlineTubeMesh(const std::vector<std::vector<glm::vec3>> &streamlines,
                                                float tubeRadius,
                                                unsigned int radialSegments);
}
