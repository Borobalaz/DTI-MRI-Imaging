#include "InspectionMovement.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
  std::unordered_map<GLFWwindow*, InspectionMovement*> g_instances;
  const glm::vec3 kWorldUp(0.0f, 1.0f, 0.0f);
}

InspectionMovement::InspectionMovement(
  GLFWwindow* window,
  const glm::vec3& lookAtPoint,
  float panSpeed,
  float orbitSensitivity,
  float zoomSensitivity,
  float minDistance,
  float maxDistance
)
  : window(window),
    previousScrollCallback(nullptr),
    lookAtPoint(lookAtPoint),
    panSpeed(panSpeed),
    orbitSensitivity(orbitSensitivity),
    zoomSensitivity(zoomSensitivity),
    minDistance(minDistance),
    maxDistance(maxDistance),
    yaw(0.0f),
    pitch(0.0f),
    distance(3.0f),
    pendingScrollOffset(0.0f),
    lastMouseX(0.0),
    lastMouseY(0.0),
    firstDrag(true),
    firstPanDrag(true),
    initialized(false)
{
  if (window != nullptr)
  {
    g_instances[window] = this;
    previousScrollCallback = glfwSetScrollCallback(window, ScrollCallbackRouter);
  }
}

InspectionMovement::~InspectionMovement()
{
  if (window != nullptr)
  {
    glfwSetScrollCallback(window, previousScrollCallback);
    g_instances.erase(window);
  }
}

void InspectionMovement::Update(
  float deltaTime,
  glm::vec3& position,
  glm::vec3& front,
  glm::vec3& up
)
{
  if (!initialized)
  {
    InitializeFromPosition(position);
    initialized = true;
  }

  ProcessOrbit();
  ProcessMousePan();
  ProcessPan(deltaTime, front, up);
  ProcessZoom();

  const float yawRadians = glm::radians(yaw);
  const float pitchRadians = glm::radians(pitch);

  glm::vec3 targetToCamera;
  targetToCamera.x = std::cos(pitchRadians) * std::cos(yawRadians);
  targetToCamera.y = std::sin(pitchRadians);
  targetToCamera.z = std::cos(pitchRadians) * std::sin(yawRadians);

  position = lookAtPoint + glm::normalize(targetToCamera) * distance;
  front = glm::normalize(lookAtPoint - position);

  const glm::vec3 right = glm::normalize(glm::cross(front, kWorldUp));
  up = glm::normalize(glm::cross(right, front));
}

void InspectionMovement::SetLookAtPoint(const glm::vec3& point)
{
  lookAtPoint = point;
}

glm::vec3 InspectionMovement::GetLookAtPoint() const
{
  return lookAtPoint;
}

void InspectionMovement::ScrollCallbackRouter(GLFWwindow* window, double xOffset, double yOffset)
{
  const auto it = g_instances.find(window);
  if (it == g_instances.end() || it->second == nullptr)
  {
    return;
  }

  InspectionMovement* const instance = it->second;
  instance->pendingScrollOffset += static_cast<float>(yOffset);

  if (instance->previousScrollCallback != nullptr)
  {
    instance->previousScrollCallback(window, xOffset, yOffset);
  }
}

void InspectionMovement::InitializeFromPosition(const glm::vec3& position)
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

void InspectionMovement::ProcessOrbit()
{
  if (window == nullptr)
  {
    return;
  }

  const bool shiftPressed =
    (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
    (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

  const bool panGestureActive =
    (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) ||
    (shiftPressed && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

  const bool orbitGestureActive =
    (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) ||
    (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

  if (panGestureActive || !orbitGestureActive)
  {
    firstDrag = true;
    return;
  }

  double mouseX = 0.0;
  double mouseY = 0.0;
  glfwGetCursorPos(window, &mouseX, &mouseY);

  if (firstDrag)
  {
    lastMouseX = mouseX;
    lastMouseY = mouseY;
    firstDrag = false;
    return;
  }

  const float deltaX = static_cast<float>(mouseX - lastMouseX);
  const float deltaY = static_cast<float>(mouseY - lastMouseY);
  lastMouseX = mouseX;
  lastMouseY = mouseY;

  yaw += deltaX * orbitSensitivity;
  pitch += deltaY * orbitSensitivity;
  pitch = std::clamp(pitch, -89.0f, 89.0f);
}

void InspectionMovement::ProcessPan(float deltaTime, const glm::vec3& front, const glm::vec3& up)
{
  if (window == nullptr)
  {
    return;
  }

  const float velocity = panSpeed * deltaTime;
  const glm::vec3 right = glm::normalize(glm::cross(front, up));

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
  {
    lookAtPoint -= right * velocity;
  }

  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
  {
    lookAtPoint += right * velocity;
  }

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
  {
    lookAtPoint += up * velocity;
  }

  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
  {
    lookAtPoint -= up * velocity;
  }
}

void InspectionMovement::ProcessZoom()
{
  if (std::abs(pendingScrollOffset) < 1e-5f)
  {
    return;
  }

  const float zoomStep = std::max(0.1f, distance * 0.1f);
  distance -= pendingScrollOffset * zoomSensitivity * zoomStep;
  distance = std::clamp(distance, minDistance, maxDistance);
  pendingScrollOffset = 0.0f;
}

void InspectionMovement::ProcessMousePan()
{
  if (window == nullptr)
  {
    return;
  }

  const bool shiftPressed =
    (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
    (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

  const bool usePanDrag =
    (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) ||
    (shiftPressed && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

  if (!usePanDrag)
  {
    firstPanDrag = true;
    return;
  }

  double mouseX = 0.0;
  double mouseY = 0.0;
  glfwGetCursorPos(window, &mouseX, &mouseY);

  if (firstPanDrag)
  {
    lastMouseX = mouseX;
    lastMouseY = mouseY;
    firstPanDrag = false;
    return;
  }

  const float deltaX = static_cast<float>(mouseX - lastMouseX);
  const float deltaY = static_cast<float>(mouseY - lastMouseY);
  lastMouseX = mouseX;
  lastMouseY = mouseY;

  const float panFactor = std::max(0.0005f, distance * 0.0015f);
  glm::vec3 targetToCamera;
  targetToCamera.x = std::cos(glm::radians(pitch)) * std::cos(glm::radians(yaw));
  targetToCamera.y = std::sin(glm::radians(pitch));
  targetToCamera.z = std::cos(glm::radians(pitch)) * std::sin(glm::radians(yaw));

  const glm::vec3 cameraToTarget = -glm::normalize(targetToCamera);
  const glm::vec3 right = glm::normalize(glm::cross(cameraToTarget, kWorldUp));
  const glm::vec3 cameraUp = glm::normalize(glm::cross(right, cameraToTarget));

  lookAtPoint -= right * (deltaX * panFactor);
  lookAtPoint += cameraUp * (deltaY * panFactor);
}