#include "VolumeGeometry.h"

VolumeGeometry::VolumeGeometry()
{
  Generate();
  Upload();
}

void VolumeGeometry::Generate()
{
  vertices =
  {
    { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f), glm::vec2(0.0f) },
    { glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec3(0.0f), glm::vec2(0.0f) },
    { glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec3(0.0f), glm::vec2(0.0f) },
    { glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(0.0f), glm::vec2(0.0f) },
    { glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3(0.0f), glm::vec2(0.0f) },
    { glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec3(0.0f), glm::vec2(0.0f) },
    { glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec3(0.0f), glm::vec2(0.0f) },
    { glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3(0.0f), glm::vec2(0.0f) }
  };

  indices =
  {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4,
    0, 4, 7, 7, 3, 0,
    1, 5, 6, 6, 2, 1,
    0, 1, 5, 5, 4, 0,
    3, 2, 6, 6, 7, 3
  };
}
