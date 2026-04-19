#include "Camera/InspectionCameraMovement.h"

#include <algorithm>
#include <cmath>

#include <Qt>

#include <glm/gtc/constants.hpp>

namespace
{
  const glm::vec3 kWorldUp(0.0f, 1.0f, 0.0f);
}

InspectionCameraMovement::InspectionCameraMovement(const glm::vec3& lookAtPoint,
                                                   float panSpeed,
                                                   float orbitSensitivity,
                                                   float zoomSensitivity,
                                                   float minDistance,
                                                   float maxDistance)
  : lookAtPoint(lookAtPoint),
    panSpeed(panSpeed),
    orbitSensitivity(orbitSensitivity),
    zoomSensitivity(zoomSensitivity),
    minDistance(minDistance),
    maxDistance(maxDistance),
    yaw(0.0f),
    pitch(0.0f),
    distance(3.0f),
    inputState(nullptr),
    lastMousePosition(0.0f, 0.0f),
    firstOrbitDrag(true),
    firstPanDrag(true),
    initialized(false),
    inputEnabled(true)
{
}

void InspectionCameraMovement::Update(float deltaTime,
                                      glm::vec3& position,
                                      glm::vec3& front,
                                      glm::vec3& up)
{
  if (!initialized)
  {
    InitializeFromPosition(position);
    initialized = true;
  }

  if (!inputEnabled || inputState == nullptr)
  {
    firstOrbitDrag = true;
    firstPanDrag = true;
    return;
  }

  const bool leftMousePressed = inputState->IsMouseButtonDown(static_cast<int>(Qt::LeftButton));
  const bool rightMousePressed = inputState->IsMouseButtonDown(static_cast<int>(Qt::RightButton));
  const bool middleMousePressed = inputState->IsMouseButtonDown(static_cast<int>(Qt::MiddleButton));
  const bool shiftPressed = inputState->IsKeyDown(Qt::Key_Shift);

  const bool panGestureActive = middleMousePressed || (shiftPressed && rightMousePressed);
  const bool orbitGestureActive = leftMousePressed || rightMousePressed;

  const glm::vec2 currentMousePosition = inputState->GetMousePosition();
  glm::vec2 mouseDelta(0.0f, 0.0f);

  if (panGestureActive)
  {
    if (firstPanDrag)
    {
      firstPanDrag = false;
      mouseDelta = glm::vec2(0.0f, 0.0f);
    }
    else
    {
      mouseDelta = currentMousePosition - lastMousePosition;
    }

    firstOrbitDrag = true;
  }
  else if (orbitGestureActive)
  {
    if (firstOrbitDrag)
    {
      firstOrbitDrag = false;
      mouseDelta = glm::vec2(0.0f, 0.0f);
    }
    else
    {
      mouseDelta = currentMousePosition - lastMousePosition;
    }

    firstPanDrag = true;
  }
  else
  {
    firstOrbitDrag = true;
    firstPanDrag = true;
  }

  lastMousePosition = currentMousePosition;

  if (orbitGestureActive && !panGestureActive)
  {
    yaw += mouseDelta.x * orbitSensitivity;
    pitch += mouseDelta.y * orbitSensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
  }

  const float yawRadians = glm::radians(yaw);
  const float pitchRadians = glm::radians(pitch);

  glm::vec3 targetToCamera;
  targetToCamera.x = std::cos(pitchRadians) * std::cos(yawRadians);
  targetToCamera.y = std::sin(pitchRadians);
  targetToCamera.z = std::cos(pitchRadians) * std::sin(yawRadians);

  targetToCamera = glm::normalize(targetToCamera);
  glm::vec3 cameraFront = glm::normalize(-targetToCamera);
  glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, kWorldUp));
  glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));

  if (panGestureActive)
  {
    const float dragPanScale = panSpeed * std::max(distance, 0.2f) * 0.0025f;
    lookAtPoint -= cameraRight * mouseDelta.x * dragPanScale;
    lookAtPoint += cameraUp * mouseDelta.y * dragPanScale;
  }

  const float keyVelocity = panSpeed * deltaTime;
  if (inputState->IsKeyDown(Qt::Key_A))
  {
    lookAtPoint -= cameraRight * keyVelocity;
  }
  if (inputState->IsKeyDown(Qt::Key_D))
  {
    lookAtPoint += cameraRight * keyVelocity;
  }
  if (inputState->IsKeyDown(Qt::Key_W))
  {
    lookAtPoint += cameraUp * keyVelocity;
  }
  if (inputState->IsKeyDown(Qt::Key_S))
  {
    lookAtPoint -= cameraUp * keyVelocity;
  }

  const float pendingScrollOffset = inputState->GetScrollDelta();
  if (std::abs(pendingScrollOffset) >= 1e-5f)
  {
    const float zoomStep = std::max(0.1f, distance * 0.1f);
    distance -= pendingScrollOffset * zoomSensitivity * zoomStep;
    distance = std::clamp(distance, minDistance, maxDistance);
  }

  position = lookAtPoint + targetToCamera * distance;
  front = glm::normalize(lookAtPoint - position);
  const glm::vec3 right = glm::normalize(glm::cross(front, kWorldUp));
  up = glm::normalize(glm::cross(right, front));
}

void InspectionCameraMovement::SetInputState(const InputState* state)
{
  inputState = state;
}

void InspectionCameraMovement::SetInputEnabled(bool enabled)
{
  inputEnabled = enabled;
}

void InspectionCameraMovement::SetLookAtPoint(const glm::vec3& point)
{
  lookAtPoint = point;
}

void InspectionCameraMovement::InitializeFromPosition(const glm::vec3& position)
{
  glm::vec3 targetToCamera = position - lookAtPoint;
  const float length = glm::length(targetToCamera);
  distance = std::clamp(length, minDistance, maxDistance);

  if (length < 1e-4f)
  {
    targetToCamera = glm::vec3(0.0f, 0.0f, 1.0f);
    distance = 1.0f;
  }
  else
  {
    targetToCamera /= length;
  }

  yaw = glm::degrees(std::atan2(targetToCamera.z, targetToCamera.x));
  pitch = glm::degrees(std::asin(std::clamp(targetToCamera.y, -1.0f, 1.0f)));
  pitch = std::clamp(pitch, -89.0f, 89.0f);
}
