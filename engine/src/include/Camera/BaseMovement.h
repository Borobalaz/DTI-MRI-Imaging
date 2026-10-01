#pragma once

#include <glm/glm.hpp>

class BaseMovement
{
public:
  virtual ~BaseMovement() = default;

  virtual void OnCameraStateChanged(const glm::vec3& position,
                                    const glm::vec3& front,
                                    const glm::vec3& up)
  {
  }

  virtual void Update(
    float deltaTime,
    glm::vec3& position,
    glm::vec3& front,
    glm::vec3& up,
    float& focalDistance,
    float& focalSize
  ) = 0;
};
