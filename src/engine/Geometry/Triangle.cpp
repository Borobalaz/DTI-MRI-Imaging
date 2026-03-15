#include "Triangle.h"

Triangle::Triangle() 
{
  Generate();
  Upload();
}

void Triangle::Generate()
{
  vertices =
  {
    { glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0.0f), glm::vec2(0.0f, 0.0f) },
    { glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec3(0.0f), glm::vec2(1.0f, 0.0f) },
    { glm::vec3( 0.0f,  0.5f, 0.0f), glm::vec3(0.0f), glm::vec2(0.5f, 1.0f) }
  };

  indices = { 0, 1, 2 };

  ComputeNormals();
}