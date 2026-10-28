#include "scene/slicing/SlicePlane.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr float kMinNormalLength = 1e-6f;

// Box corner index pairs that differ in exactly one bit: the 12 box edges.
constexpr std::array<std::array<int, 2>, 12> kBoxEdges{{
  {0, 1}, {2, 3}, {4, 5}, {6, 7},  // x
  {0, 2}, {1, 3}, {4, 6}, {5, 7},  // y
  {0, 4}, {1, 5}, {2, 6}, {3, 7}   // z
}};

float MaxComponent(const glm::vec3& value)
{
  return std::max({value.x, value.y, value.z});
}

float ClampToLimit(float value, float limit)
{
  return std::clamp(value, -limit, limit);
}
}

glm::vec3 DatasetObjectExtents(const glm::ivec3& dimensions, const glm::vec3& spacing)
{
  const glm::vec3 extents = glm::max(glm::vec3(dimensions) * spacing, glm::vec3(1e-6f));
  return extents / MaxComponent(extents);
}

float DatasetVoxelStep(const glm::ivec3& dimensions, const glm::vec3& spacing)
{
  const glm::vec3 extents = glm::max(glm::vec3(dimensions) * spacing, glm::vec3(1e-6f));
  return std::max(std::min({spacing.x, spacing.y, spacing.z}), 1e-6f) / MaxComponent(extents);
}

bool SlicePlane::SetNormal(const glm::vec3& newNormal)
{
  if (!std::isfinite(newNormal.x) || !std::isfinite(newNormal.y) || !std::isfinite(newNormal.z))
  {
    return false;
  }

  const float length = glm::length(newNormal);
  if (length < kMinNormalLength)
  {
    return false;
  }

  const glm::vec3 unitNormal = newNormal / length;
  const float clampedOffset = ClampToLimit(offset, glm::dot(glm::abs(unitNormal), boxHalfExtents));
  if (unitNormal == normal && clampedOffset == offset)
  {
    return true;
  }

  normal = unitNormal;
  offset = clampedOffset;
  ++version;
  return true;
}

void SlicePlane::SetOffset(float newOffset)
{
  if (!std::isfinite(newOffset))
  {
    return;
  }

  const float clampedOffset = ClampToLimit(newOffset, GetOffsetLimit());
  if (clampedOffset != offset)
  {
    offset = clampedOffset;
    ++version;
  }
}

void SlicePlane::SetBoxHalfExtents(const glm::vec3& halfExtents)
{
  glm::vec3 sanitised(0.0f);
  for (int axis = 0; axis < 3; ++axis)
  {
    const float component = halfExtents[axis];
    sanitised[axis] = std::isfinite(component) ? std::max(component, 0.0f) : 0.0f;
  }

  const float clampedOffset = ClampToLimit(offset, glm::dot(glm::abs(normal), sanitised));
  if (sanitised == boxHalfExtents && clampedOffset == offset)
  {
    return;
  }

  boxHalfExtents = sanitised;
  offset = clampedOffset;
  ++version;
}

float SlicePlane::GetOffsetLimit() const
{
  return glm::dot(glm::abs(normal), boxHalfExtents);
}

float SlicePlane::SignedDistance(const glm::vec3& p) const
{
  return glm::dot(normal, p) - offset;
}

PlaneBasis SlicePlane::GetInPlaneBasis() const
{
  // Ties go to the lowest axis so the choice is deterministic and the glyph grid does not jump.
  const glm::vec3 magnitude = glm::abs(normal);
  glm::vec3 helperAxis(0.0f, 0.0f, 1.0f);
  if (magnitude.x <= magnitude.y && magnitude.x <= magnitude.z)
  {
    helperAxis = glm::vec3(1.0f, 0.0f, 0.0f);
  }
  else if (magnitude.y <= magnitude.z)
  {
    helperAxis = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  PlaneBasis basis;
  basis.u = glm::normalize(helperAxis - glm::dot(helperAxis, normal) * normal);
  basis.v = glm::cross(normal, basis.u);
  return basis;
}

std::vector<glm::vec3> SlicePlane::IntersectBox(const glm::vec3& halfExtents) const
{
  const glm::vec3 h = glm::max(halfExtents, glm::vec3(0.0f));
  const float scale = std::max(MaxComponent(h), 1e-6f);
  const float snapEpsilon = 1e-6f * scale;
  const float mergeEpsilon = 1e-5f * scale;

  std::array<glm::vec3, 8> corners;
  std::array<float, 8> distances;
  for (int i = 0; i < 8; ++i)
  {
    corners[i] = glm::vec3((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z);
    const float distance = SignedDistance(corners[i]);
    distances[i] = std::abs(distance) <= snapEpsilon ? 0.0f : distance;
  }

  std::vector<glm::vec3> candidates;
  candidates.reserve(24);
  for (const std::array<int, 2>& edge : kBoxEdges)
  {
    const int a = edge[0];
    const int b = edge[1];
    if (distances[a] == 0.0f)
    {
      candidates.push_back(corners[a]);
    }
    if (distances[b] == 0.0f)
    {
      candidates.push_back(corners[b]);
    }
    if ((distances[a] < 0.0f && distances[b] > 0.0f) || (distances[a] > 0.0f && distances[b] < 0.0f))
    {
      const float t = distances[a] / (distances[a] - distances[b]);
      candidates.push_back(corners[a] + t * (corners[b] - corners[a]));
    }
  }

  std::vector<glm::vec3> polygon;
  for (const glm::vec3& candidate : candidates)
  {
    const bool duplicate = std::any_of(polygon.begin(), polygon.end(),
      [&](const glm::vec3& kept) { return glm::length(candidate - kept) <= mergeEpsilon; });
    if (!duplicate)
    {
      polygon.push_back(candidate);
    }
  }

  if (polygon.size() < 3)
  {
    return {};
  }

  // A plane/box section is convex, so angular order around the centroid is polygon order;
  // cross(u, v) == normal makes ascending angle counter-clockwise seen from +normal.
  glm::vec3 centroid(0.0f);
  for (const glm::vec3& point : polygon)
  {
    centroid += point;
  }
  centroid /= static_cast<float>(polygon.size());

  const PlaneBasis basis = GetInPlaneBasis();
  auto angleAroundCentroid = [&](const glm::vec3& point)
  {
    const glm::vec3 fromCentroid = point - centroid;
    return std::atan2(glm::dot(fromCentroid, basis.v), glm::dot(fromCentroid, basis.u));
  };
  std::sort(polygon.begin(), polygon.end(),
    [&](const glm::vec3& left, const glm::vec3& right)
    {
      return angleAroundCentroid(left) < angleAroundCentroid(right);
    });

  return polygon;
}

PlaneEquation SlicePlane::ToUnitCubeSpace(const glm::vec3& objectExtents) const
{
  // p = E * q pulls the plane back through the diagonal scale: dot(E * n, q) = offset.
  const glm::vec3 scaledNormal = objectExtents * normal;
  const float length = glm::length(scaledNormal);
  if (length < 1e-12f)
  {
    return PlaneEquation{normal, offset};
  }
  return PlaneEquation{scaledNormal / length, offset / length};
}
