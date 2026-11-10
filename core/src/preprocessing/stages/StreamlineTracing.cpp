#include "Preprocessing/stages/StreamlineTracing.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "scene/geometry/TubeGeometry.h"
#include "scene/object/Mesh.h"
#include "scene/volume/VolumeData.h"

namespace
{
  float Lerp(float a, float b, float t)
  {
    return a + (b - a) * t;
  }

  // Forward difference, or backward difference at the last point; +y when degenerate.
  glm::vec3 EstimateTangent(const std::vector<glm::vec3> &streamline, size_t pointIndex)
  {
    const glm::vec3 &point = streamline[pointIndex];
    glm::vec3 tangent(0.0f, 1.0f, 0.0f);
    if (pointIndex + 1 < streamline.size())
    {
      tangent = streamline[pointIndex + 1] - point;
    }
    else if (pointIndex > 0)
    {
      tangent = point - streamline[pointIndex - 1];
    }

    const float tangentLength = glm::length(tangent);
    if (tangentLength > 1e-6f)
    {
      return tangent / tangentLength;
    }
    return glm::vec3(0.0f, 1.0f, 0.0f);
  }
}

namespace StreamlineTracing
{
  /**
   * @brief Sample a scalar volume using trilinear interpolation in voxel space.
   *
   * @param voxels Flat voxel buffer.
   * @param point Sample position in voxel coordinates.
   * @param dims Volume dimensions.
   * @return Interpolated scalar value.
   */
  float SampleTrilinear(const std::vector<float> &voxels,
                        const glm::vec3 &point,
                        const glm::ivec3 &dims)
  {
    if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0)
    {
      return 0.0f;
    }

    const float maxX = static_cast<float>(std::max(dims.x - 1, 0));
    const float maxY = static_cast<float>(std::max(dims.y - 1, 0));
    const float maxZ = static_cast<float>(std::max(dims.z - 1, 0));

    const float x = std::clamp(point.x, 0.0f, maxX);
    const float y = std::clamp(point.y, 0.0f, maxY);
    const float z = std::clamp(point.z, 0.0f, maxZ);

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = std::min(x0 + 1, dims.x - 1);
    const int y1 = std::min(y0 + 1, dims.y - 1);
    const int z1 = std::min(z0 + 1, dims.z - 1);

    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const float tz = z - static_cast<float>(z0);

    const float c000 = voxels[VolumeData::FlatIndex(x0, y0, z0, dims.x, dims.y)];
    const float c100 = voxels[VolumeData::FlatIndex(x1, y0, z0, dims.x, dims.y)];
    const float c010 = voxels[VolumeData::FlatIndex(x0, y1, z0, dims.x, dims.y)];
    const float c110 = voxels[VolumeData::FlatIndex(x1, y1, z0, dims.x, dims.y)];
    const float c001 = voxels[VolumeData::FlatIndex(x0, y0, z1, dims.x, dims.y)];
    const float c101 = voxels[VolumeData::FlatIndex(x1, y0, z1, dims.x, dims.y)];
    const float c011 = voxels[VolumeData::FlatIndex(x0, y1, z1, dims.x, dims.y)];
    const float c111 = voxels[VolumeData::FlatIndex(x1, y1, z1, dims.x, dims.y)];

    const float c00 = Lerp(c000, c100, tx);
    const float c10 = Lerp(c010, c110, tx);
    const float c01 = Lerp(c001, c101, tx);
    const float c11 = Lerp(c011, c111, tx);
    const float c0 = Lerp(c00, c10, ty);
    const float c1 = Lerp(c01, c11, ty);
    const float value = Lerp(c0, c1, tz);
    return std::isfinite(value) ? value : 0.0f;
  }

  /**
   * @brief Check if a point in voxel space is within the bounds of the volume dimensions.
   *
   * @param point Point in voxel coordinates.
   * @param dims Volume dimensions.
   * @return true if the point lies inside the volume bounds, false otherwise.
   */
  bool IsInBounds(const glm::vec3 &point, const glm::ivec3 &dims)
  {
    return point.x >= 0.0f && point.y >= 0.0f && point.z >= 0.0f &&
           point.x <= static_cast<float>(dims.x - 1) &&
           point.y <= static_cast<float>(dims.y - 1) &&
           point.z <= static_cast<float>(dims.z - 1);
  }

  /**
   * @brief Convert a position in voxel space to object space.
   *
   * @param voxelPosition Position in voxel coordinates.
   * @param dims Volume dimensions.
   * @param spacing Voxel spacing.
   * @return Corresponding object-space position.
   */
  glm::vec3 VoxelToObjectSpace(const glm::vec3 &voxelPosition,
                               const glm::ivec3 &dims,
                               const glm::vec3 &spacing)
  {
    const glm::vec3 physicalPos = voxelPosition * spacing;
    const glm::vec3 extent = glm::vec3(std::max(dims.x - 1, 1),
                                       std::max(dims.y - 1, 1),
                                       std::max(dims.z - 1, 1)) *
                             spacing;
    const float maxExtent = std::max(std::max(extent.x, extent.y), extent.z);
    if (maxExtent <= 1e-6f)
    {
      return glm::vec3(0.0f);
    }

    const glm::vec3 centered = physicalPos - extent * 0.5f;
    return centered / maxExtent;
  }

  /**
   * @brief Convert a unit physical-space direction into a unit voxel-space step direction, so a
   *        step of N voxels covers the same physical distance along every axis.
   *
   * @param unitDirection Unit direction in physical space.
   * @param spacing Voxel spacing.
   * @return Unit voxel-space direction, or the zero vector when degenerate.
   */
  glm::vec3 ToIndexSpaceStepDirection(const glm::vec3 &unitDirection, const glm::vec3 &spacing)
  {
    const glm::vec3 indexSpaceDirection(
        unitDirection.x / std::max(spacing.x, 1e-6f),
        unitDirection.y / std::max(spacing.y, 1e-6f),
        unitDirection.z / std::max(spacing.z, 1e-6f));

    const float indexLength = glm::length(indexSpaceDirection);
    if (indexLength <= 1e-6f)
    {
      return glm::vec3(0.0f);
    }

    return indexSpaceDirection / indexLength;
  }

  /**
   * @brief Join the two halves of a bidirectionally traced streamline.
   *
   * @param backward Half traced against the initial direction, starting at the seed.
   * @param forward Half traced along the initial direction, starting at the seed.
   * @return Backward half reversed followed by the forward half, with the seed point once.
   */
  std::vector<glm::vec3> JoinBidirectional(const std::vector<glm::vec3> &backward,
                                           const std::vector<glm::vec3> &forward)
  {
    std::vector<glm::vec3> joined(backward.rbegin(), backward.rend());
    if (!joined.empty() && !forward.empty())
    {
      // Both halves start at the seed; keep the forward copy.
      joined.pop_back();
    }

    joined.insert(joined.end(), forward.begin(), forward.end());
    return joined;
  }

  /**
   * @brief Build one tube mesh from object-space streamline polylines, separated by
   *        primitive-restart indices so the whole set draws in a single call.
   *
   * @param streamlines Object-space polylines.
   * @param tubeRadius Tube radius in object space.
   * @param radialSegments Tube cross-section segment count.
   * @return Streamline mesh, or nullptr if there are no points.
   */
  std::shared_ptr<Mesh> BuildStreamlineTubeMesh(const std::vector<std::vector<glm::vec3>> &streamlines,
                                                float tubeRadius,
                                                unsigned int radialSegments)
  {
    size_t pointCount = 0;
    for (const std::vector<glm::vec3> &streamline : streamlines)
    {
      pointCount += streamline.size();
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(pointCount);
    indices.reserve(pointCount + streamlines.size());

    for (const std::vector<glm::vec3> &streamline : streamlines)
    {
      if (streamline.empty())
      {
        continue;
      }

      const unsigned int startIndex = static_cast<unsigned int>(vertices.size());
      for (size_t i = 0; i < streamline.size(); ++i)
      {
        const glm::vec3 tangent = EstimateTangent(streamline, i);

        Vertex v{};
        v.position = streamline[i];
        v.normal = tangent;
        v.tangent = tangent;
        v.texCoord = glm::vec2(0.0f, 0.0f);
        vertices.push_back(v);
        indices.push_back(startIndex + static_cast<unsigned int>(i));
      }

      // Restart index after each streamline keeps them visually separate within one draw call.
      indices.push_back(std::numeric_limits<unsigned int>::max());
    }

    if (indices.empty() || vertices.empty())
    {
      return nullptr;
    }

    if (indices.back() == std::numeric_limits<unsigned int>::max())
    {
      indices.pop_back();
    }

    std::shared_ptr<Geometry> geometry = std::make_shared<TubeGeometry>(
        std::move(vertices),
        std::move(indices),
        tubeRadius,
        radialSegments);

    // Material is assigned by the scene to keep preprocessing render-policy agnostic.
    return std::make_shared<Mesh>(geometry, nullptr);
  }
}
