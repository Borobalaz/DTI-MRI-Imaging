#pragma once

#include <glm/glm.hpp>

class BaseMovement
{
public:
  virtual ~BaseMovement() = default;

  virtual void Update(
    float deltaTime,
    glm::vec3& position,
    glm::vec3& front,
    glm::vec3& up
  ) = 0;
};
