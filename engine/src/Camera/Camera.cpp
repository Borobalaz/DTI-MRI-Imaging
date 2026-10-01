#include "Camera/Camera.h"

#include <algorithm>
#include <string>

#include "Shader.h"

/**
 * @brief Construct a new Camera:: Camera object
 * 
 */
Camera::Camera()
  : transform(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f)),
    front(0.0f, 0.0f, -1.0f),
    up(0.0f, 1.0f, 0.0f),
    aspect(4.0f / 3.0f),
    focalDistance(1.0f),
    focalSize(1.0f)
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
  const std::string focalPointUniformName =
    ComposeUniformName("camera", "focalPoint");
  const std::string focalSizeUniformName =
    ComposeUniformName("camera", "focalSize");

  shader.SetMat4(viewUniformName, GetViewMatrix());

  // Also set plain uniform names if the shader expects them (useful for legacy or specialized shaders
  // such as the skybox shader which expects 'viewMatrix' and 'projectionMatrix').
  shader.SetMat4("viewMatrix", GetViewMatrix());

  shader.SetMat4(projectionUniformName, GetProjectionMatrix());

  shader.SetMat4("projectionMatrix", GetProjectionMatrix());

  const glm::vec3 forward = GetForwardVector();

  shader.SetVec3(viewPositionUniformName, GetPosition());

  shader.SetVec3(focalPointUniformName, GetPosition() + forward * focalDistance);

  shader.SetFloat(focalSizeUniformName, focalSize);
}

std::string Camera::GetInspectDisplayName() const
{
  return "camera";
}

std::vector<InspectFieldPtr> Camera::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields = transform.GetInspectFields();

  auto aspectField = MakeInspectField(
    "aspect",
    "Aspect",
    "Projection",
    InspectFieldType::Number,
    static_cast<double>(aspect),
    [this]() -> InspectValue { return static_cast<double>(aspect); },
    [this](const InspectValue& value)
    {
      if (const auto *number = std::get_if<double>(&value))
        SetAspect(static_cast<float>(*number));
    }
  );
  aspectField->minimum = 0.01;
  aspectField->maximum = 100.0;
  aspectField->step = 0.01;

  auto focalDistanceField = MakeInspectField(
    "focalDistance",
    "Focal Distance",
    "Focus",
    InspectFieldType::Number,
    static_cast<double>(focalDistance),
    [this]() -> InspectValue { return static_cast<double>(focalDistance); },
    [this](const InspectValue& value)
    {
      if (const auto *number = std::get_if<double>(&value))
        SetFocalDistance(static_cast<float>(*number));
    }
  );
  focalDistanceField->minimum = 0.01;
  focalDistanceField->maximum = 1000.0;
  focalDistanceField->step = 0.1;

  auto focalSizeField = MakeInspectField(
    "focalSize",
    "Focal Size",
    "Focus",
    InspectFieldType::Number,
    static_cast<double>(focalSize),
    [this]() -> InspectValue { return static_cast<double>(focalSize); },
    [this](const InspectValue& value)
    {
      if (const auto *number = std::get_if<double>(&value))
        SetFocalSize(static_cast<float>(*number));
    }
  );
  focalSizeField->minimum = 0.01;
  focalSizeField->maximum = 1000.0;
  focalSizeField->step = 0.1;

  auto movementComponentField = MakeInspectField(
    "moveComponent",
    "Move Component",
    "Runtime",
    InspectFieldType::Text,
    moveComponent ? InspectValue{"Attached"} : InspectValue{"None"}
  );
  movementComponentField->readOnly = true;

  fields.push_back(aspectField);
  fields.push_back(focalDistanceField);
  fields.push_back(focalSizeField);
  fields.push_back(movementComponentField);
  return fields;
}

/**
 * @brief Set the camera's position
 * 
 * @param pos 
 */
void Camera::SetPosition(const glm::vec3& pos)
{
  transform.SetPosition(pos);
  NotifyMovementCameraStateChanged();
}

/**
 * @brief Get the camera's current position
 * 
 * @return glm::vec3 
 */
glm::vec3 Camera::GetPosition() const
{
  return transform.GetPosition();
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

void Camera::SetFocalDistance(float newFocalDistance)
{
  focalDistance = std::max(0.01f, newFocalDistance);
}

float Camera::GetFocalDistance() const
{
  return focalDistance;
}

void Camera::SetFocalSize(float newFocalSize)
{
  focalSize = std::max(0.01f, newFocalSize);
}

float Camera::GetFocalSize() const
{
  return focalSize;
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
    front = GetForwardVector();
    glm::vec3 position = transform.GetPosition();
    moveComponent->Update(deltaTime, position, front, up, focalDistance, focalSize);
    transform.SetPosition(position);
    transform.SetOrientation(front);
  }
}

void Camera::NotifyMovementCameraStateChanged()
{
  if (moveComponent)
  {
    moveComponent->OnCameraStateChanged(transform.GetPosition(), front, up);
  }
}

glm::vec3 Camera::GetForwardVector() const
{
  const glm::vec3 orientation = transform.GetOrientation();
  if (glm::length(orientation) > 1e-5f)
  {
    return glm::normalize(orientation);
  }

  return front;
}