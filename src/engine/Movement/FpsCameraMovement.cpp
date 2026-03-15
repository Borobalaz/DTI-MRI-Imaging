#include "FpsCameraMovement.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

FpsCameraMovement::FpsCameraMovement(
  GLFWwindow* window,
  float speed,
  float mouseSensitivity
)
  : window(window),
    speed(speed),
    sensitivity(mouseSensitivity),
    yaw(-90.0f),
    pitch(0.0f),
    lastMouseX(0.0),
    lastMouseY(0.0),
    firstMouse(true)
{
}

void FpsCameraMovement::Update(
  float deltaTime,
  glm::vec3& position,
  glm::vec3& front,
  glm::vec3& up
)
{
  ProcessMouse(front);
  ProcessKeyboard(deltaTime, position, front, up);
}

void FpsCameraMovement::ProcessKeyboard(
  float deltaTime,
  glm::vec3& position,
  const glm::vec3& front,
  const glm::vec3& up
)
{
  const glm::vec3 right = glm::normalize(glm::cross(front, up));
  const float velocity = speed * deltaTime;

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    position += front * velocity;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    position -= front * velocity;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    position -= right * velocity;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    position += right * velocity;
}

void FpsCameraMovement::ProcessMouse(glm::vec3& front)
{
  double mouseX, mouseY;
  glfwGetCursorPos(window, &mouseX, &mouseY);

  if (firstMouse)
  {
    lastMouseX = mouseX;
    lastMouseY = mouseY;
    firstMouse = false;
  }

  const float deltaX = static_cast<float>(mouseX - lastMouseX) * sensitivity;
  const float deltaY = static_cast<float>(lastMouseY - mouseY) * sensitivity;

  lastMouseX = mouseX;
  lastMouseY = mouseY;

  yaw   += deltaX;
  pitch += deltaY;
  pitch  = std::clamp(pitch, -89.0f, 89.0f);

  glm::vec3 newFront;
  newFront.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
  newFront.y = std::sin(glm::radians(pitch));
  newFront.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
  front = glm::normalize(newFront);
}

void FpsCameraMovement::SetSpeed(float newSpeed)
{
  speed = newSpeed;
}

void FpsCameraMovement::SetMouseSensitivity(float newSensitivity)
{
  sensitivity = newSensitivity;
}
