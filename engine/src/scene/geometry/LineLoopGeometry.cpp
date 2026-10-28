#include "scene/geometry/LineLoopGeometry.h"

LineLoopGeometry::LineLoopGeometry()
{
  SetPrimitive(PrimitiveTopology::LineStrip);
  // Core profiles only guarantee a line width of 1.
  SetLineWidth(1.0f);
}

void LineLoopGeometry::SetLoop(const std::vector<glm::vec3>& points, const glm::vec3& normal)
{
  vertices.clear();
  indices.clear();

  if (points.size() >= 3)
  {
    vertices.reserve(points.size());
    indices.reserve(points.size() + 1);
    for (size_t i = 0; i < points.size(); ++i)
    {
      vertices.push_back(Vertex{points[i], normal, glm::vec2(0.0f), glm::vec3(0.0f)});
      indices.push_back(static_cast<unsigned int>(i));
    }
    // Repeating the first index closes the loop.
    indices.push_back(0);
  }

  MarkGpuDirty();
}

void LineLoopGeometry::Generate()
{
}
