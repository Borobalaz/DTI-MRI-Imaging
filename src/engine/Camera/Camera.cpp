#include "Camera.h"

#include "Shader.h"

/**
 * @brief Construct a new Camera:: Camera object
 * 
 */
Camera::Camera()
  : position(0.0f, 0.0f, 3.0f),
    front(0.0f, 0.0f, -1.0f),
    up(0.0f, 1.0f, 0.0f),
    aspect(4.0f / 3.0f)
{
}

/**
 * @brief Destroy the Camera:: Camera object
 * 
 */
Camera::~Camera()
{
}

/**
 * @brief Apply the camera's view and projection matrices to the shader
 * 
 * @param shader 
 */
void Camera::Apply(Shader& shader) const
{
  const std::string viewUniformName =
    ComposeUniformName("camera", "viewMatrix");
  const std::string projectionUniformName =
    ComposeUniformName("camera", "projectionMatrix");
  const std::string viewPositionUniformName =
    ComposeUniformName("camera", "viewPosition");

  if (shader.HasUniform(viewUniformName))
  {
    shader.SetMat4(viewUniformName, GetViewMatrix());
  }

  if (shader.HasUniform(projectionUniformName))
  {
    shader.SetMat4(projectionUniformName, GetProjectionMatrix());
  }

  if (shader.HasUniform(viewPositionUniformName))
  {
    shader.SetVec3(viewPositionUniformName, GetPosition());
  }
}

/**
 * @brief Set the camera's position
 * 
 * @param pos 
 */
void Camera::SetPosition(const glm::vec3& pos)
{
  position = pos;
}

/**
 * @brief Get the camera's current position
 * 
 * @return glm::vec3 
 */
glm::vec3 Camera::GetPosition() const
{
  return position;
}

void Camera::SetAspect(float newAspect)
{
  if (newAspect > 0.0f)
  {
    aspect = newAspect;
  }
}

float Camera::GetAspect() const
{
  return aspect;
}

/**
 * @brief Set the camera movement component
 * 
 * @param component 
 */
void Camera::SetMoveComponent(std::unique_ptr<BaseMovement> component)
{
  moveComponent = std::move(component);
}

/**
 * @brief Apply the camera movement component
 * 
 * @param deltaTime 
 */
void Camera::Move(float deltaTime)
{
  if (moveComponent)
  {
    moveComponent->Update(deltaTime, position, front, up);
  }
}