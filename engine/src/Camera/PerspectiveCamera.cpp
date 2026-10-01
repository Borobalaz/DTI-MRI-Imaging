#include "Camera/PerspectiveCamera.h"
#include <glm/gtc/matrix_transform.hpp>

PerspectiveCamera::PerspectiveCamera(
  float fov,
  float aspect,
  float nearPlane,
  float farPlane
)
  : fov(fov),
    nearPlane(nearPlane),
    farPlane(farPlane)
{
  transform.SetPosition(glm::vec3(0.0f, 0.0f, 3.0f));
  SetAspect(aspect);
}

PerspectiveCamera::~PerspectiveCamera()
{
}

glm::mat4 PerspectiveCamera::GetViewMatrix() const
{
  const glm::vec3 forward = GetForwardVector();
  return glm::lookAt(
    GetPosition(),
    GetPosition() + forward,
    up
  );
}

glm::mat4 PerspectiveCamera::GetProjectionMatrix() const
{
  return glm::perspective(
    glm::radians(fov),
    GetAspect(),
    nearPlane,
    farPlane
  );
}

void PerspectiveCamera::Update(float deltaTime)
{
  Move(deltaTime);
}

void PerspectiveCamera::LookAt(const glm::vec3& target)
{
  front = glm::normalize(target - GetPosition());
  transform.SetOrientation(front);
}