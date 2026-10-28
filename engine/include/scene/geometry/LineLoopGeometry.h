#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "render/geometry/Geometry.h"

// One closed polyline drawn as a line strip (no primitive restart). Replace the points only on the
// engine (GL) thread; the GPU buffers re-upload lazily at the next draw.
class LineLoopGeometry final : public Geometry
{
public:
  LineLoopGeometry();

  // `points`: ordered loop, first vertex not repeated. Fewer than 3 points clears the loop (draws
  // nothing). `normal` is written into every vertex.
  void SetLoop(const std::vector<glm::vec3>& points, const glm::vec3& normal);

  void Generate() override;
};
