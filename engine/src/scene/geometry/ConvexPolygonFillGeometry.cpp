#include "scene/geometry/ConvexPolygonFillGeometry.h"

void ConvexPolygonFillGeometry::SetPolygon(const std::vector<glm::vec3>& points, const glm::vec3& normal)
{
  vertices.clear();
  indices.clear();

  if (points.size() >= 3)
  {
    vertices.reserve(points.size());
    for (const glm::vec3& point : points)
    {
      vertices.push_back(Vertex{point, normal, glm::vec2(0.0f), glm::vec3(0.0f)});
    }

    // Fan around vertex 0: keeps the polygon's winding (CCW seen from +normal).
    indices.reserve(3 * (points.size() - 2));
    for (unsigned int i = 1; i + 1 < static_cast<unsigned int>(points.size()); ++i)
    {
      indices.push_back(0);
      indices.push_back(i);
      indices.push_back(i + 1);
    }
  }

  MarkGpuDirty();
}

void ConvexPolygonFillGeometry::Generate()
{
}
