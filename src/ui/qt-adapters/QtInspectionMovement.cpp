#include "ui/qt-adapters/QtInspectionMovement.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include <glm/gtc/constants.hpp>

namespace
{
  const glm::vec3 kWorldUp(0.0f, 1.0f, 0.0f);
}

QtInspectionMovement::QtInspectionMovement(const glm::vec3 &lookAtPoint,
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
      pendingScrollOffset(0.0f),
      lastMousePosition(0.0, 0.0),
      firstOrbitDrag(true),
      firstPanDrag(true),
      initialized(false),
      inputEnabled(true),
      leftMousePressed(false),
      rightMousePressed(false),
      middleMousePressed(false),
      accumulatedOrbitDeltaX(0.0f),
      accumulatedOrbitDeltaY(0.0f),
      accumulatedPanDeltaX(0.0f),
      accumulatedPanDeltaY(0.0f)
{
}

void QtInspectionMovement::Update(float deltaTime,
                                  glm::vec3 &position,
                                  glm::vec3 &front,
                                  glm::vec3 &up)
{
  std::lock_guard<std::mutex> lock(mutex);

  if (!initialized)
  {
    InitializeFromPosition(position);
    initialized = true;
  }

  if (!inputEnabled)
  {
    firstOrbitDrag = true;
    firstPanDrag = true;
    accumulatedOrbitDeltaX = 0.0f;
    accumulatedOrbitDeltaY = 0.0f;
    accumulatedPanDeltaX = 0.0f;
    accumulatedPanDeltaY = 0.0f;
    pendingScrollOffset = 0.0f;
    return;
  }

  yaw += accumulatedOrbitDeltaX * orbitSensitivity;
  pitch += accumulatedOrbitDeltaY * orbitSensitivity;
  pitch = std::clamp(pitch, -89.0f, 89.0f);
  accumulatedOrbitDeltaX = 0.0f;
  accumulatedOrbitDeltaY = 0.0f;

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

  const float dragPanScale = panSpeed * std::max(distance, 0.2f) * 0.0025f;
  lookAtPoint -= cameraRight * accumulatedPanDeltaX * dragPanScale;
  lookAtPoint += cameraUp * accumulatedPanDeltaY * dragPanScale;
  accumulatedPanDeltaX = 0.0f;
  accumulatedPanDeltaY = 0.0f;

  const float keyVelocity = panSpeed * deltaTime;
  if (pressedKeys.count(Qt::Key_A) > 0)
  {
    lookAtPoint -= cameraRight * keyVelocity;
  }
  if (pressedKeys.count(Qt::Key_D) > 0)
  {
    lookAtPoint += cameraRight * keyVelocity;
  }
  if (pressedKeys.count(Qt::Key_W) > 0)
  {
    lookAtPoint -= cameraUp * keyVelocity;
  }
  if (pressedKeys.count(Qt::Key_S) > 0)
  {
    lookAtPoint += cameraUp * keyVelocity;
  }

  if (std::abs(pendingScrollOffset) >= 1e-5f)
  {
    const float zoomStep = std::max(0.1f, distance * 0.1f);
    distance -= pendingScrollOffset * zoomSensitivity * zoomStep;
    distance = std::clamp(distance, minDistance, maxDistance);
    pendingScrollOffset = 0.0f;
  }

  position = lookAtPoint + targetToCamera * distance;
  front = glm::normalize(lookAtPoint - position);
  const glm::vec3 right = glm::normalize(glm::cross(front, kWorldUp));
  up = glm::normalize(glm::cross(right, front));
}

void QtInspectionMovement::SetInputEnabled(bool enabled)
{
  std::lock_guard<std::mutex> lock(mutex);
  inputEnabled = enabled;
}

void QtInspectionMovement::SetLookAtPoint(const glm::vec3 &point)
{
  std::lock_guard<std::mutex> lock(mutex);
  lookAtPoint = point;
}

void QtInspectionMovement::SetKeyPressed(int key, bool pressed)
{
  std::lock_guard<std::mutex> lock(mutex);

  if (pressed)
  {
    pressedKeys.insert(key);
  }
  else
  {
    pressedKeys.erase(key);
  }
}

void QtInspectionMovement::SetMouseButtonPressed(Qt::MouseButton button, bool pressed)
{
  std::lock_guard<std::mutex> lock(mutex);

  if (button == Qt::LeftButton)
  {
    leftMousePressed = pressed;
    if (!pressed)
    {
      firstOrbitDrag = true;
    }
  }
  else if (button == Qt::RightButton)
  {
    rightMousePressed = pressed;
    if (!pressed)
    {
      firstOrbitDrag = true;
      firstPanDrag = true;
    }
  }
  else if (button == Qt::MiddleButton)
  {
    middleMousePressed = pressed;
    if (!pressed)
    {
      firstPanDrag = true;
    }
  }
}

void QtInspectionMovement::SetMousePosition(const QPointF &position)
{
  std::lock_guard<std::mutex> lock(mutex);

  const bool shiftPressed = pressedKeys.count(Qt::Key_Shift) > 0;
  const bool panGestureActive = middleMousePressed || (shiftPressed && rightMousePressed);
  const bool orbitGestureActive = leftMousePressed || rightMousePressed;

  if (panGestureActive)
  {
    if (firstPanDrag)
    {
      lastMousePosition = position;
      firstPanDrag = false;
    }
    else
    {
      const QPointF delta = position - lastMousePosition;
      accumulatedPanDeltaX += static_cast<float>(delta.x());
      accumulatedPanDeltaY -= static_cast<float>(delta.y());
      lastMousePosition = position;
    }

    firstOrbitDrag = true;
    return;
  }

  if (orbitGestureActive)
  {
    if (firstOrbitDrag)
    {
      lastMousePosition = position;
      firstOrbitDrag = false;
    }
    else
    {
      const QPointF delta = position - lastMousePosition;
      accumulatedOrbitDeltaX += static_cast<float>(delta.x());
      accumulatedOrbitDeltaY -= static_cast<float>(delta.y());
      lastMousePosition = position;
    }

    firstPanDrag = true;
    return;
  }

  firstOrbitDrag = true;
  firstPanDrag = true;
  lastMousePosition = position;
}

void QtInspectionMovement::AddScrollDelta(float delta)
{
  std::lock_guard<std::mutex> lock(mutex);
  pendingScrollOffset += delta;
}

void QtInspectionMovement::InitializeFromPosition(const glm::vec3 &position)
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
