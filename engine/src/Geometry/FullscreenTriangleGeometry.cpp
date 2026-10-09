#include "Geometry/FullscreenTriangleGeometry.h"

FullscreenTriangleGeometry::FullscreenTriangleGeometry()
{
  this->Generate();
  this->Upload();
}

void FullscreenTriangleGeometry::Generate()
{
  vertices =
  {
    { glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f) },
    { glm::vec3( 3.0f, -1.0f, 0.0f), glm::vec3(0.0f), glm::vec2(2.0f, 0.0f), glm::vec3(0.0f) },
    { glm::vec3(-1.0f,  3.0f, 0.0f), glm::vec3(0.0f), glm::vec2(0.0f, 2.0f), glm::vec3(0.0f) }
  };

  indices = { 0, 1, 2 };
}
