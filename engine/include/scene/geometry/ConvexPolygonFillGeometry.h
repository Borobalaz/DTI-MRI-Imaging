#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "render/geometry/Geometry.h"

// A convex, ordered polygon triangulated as an indexed triangle fan. Replace the points only on the
// engine (GL) thread; the GPU buffers re-upload lazily at the next draw.
class ConvexPolygonFillGeometry final : public Geometry
{
public:
  ConvexPolygonFillGeometry() = default; // default topology: Triangles

  // `points`: ordered convex polygon, first vertex not repeated. Fewer than 3 points clears the
  // polygon (draws nothing). `normal` is written into every vertex.
  void SetPolygon(const std::vector<glm::vec3>& points, const glm::vec3& normal);

  void Generate() override;
};
