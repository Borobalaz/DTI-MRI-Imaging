#pragma once

#include <GLFW/glfw3.h>

#include "BaseMovement.h"

class FpsCameraMovement : public BaseMovement
{
public:
  FpsCameraMovement(
    GLFWwindow* window,
    float speed = 2.5f,
    float mouseSensitivity = 0.1f
  );

  void Update(
    float deltaTime,
    glm::vec3& position,
    glm::vec3& front,
    glm::vec3& up
  ) override;

  void SetSpeed(float speed);
  void SetMouseSensitivity(float sensitivity);

private:
  void ProcessKeyboard(float deltaTime, glm::vec3& position, const glm::vec3& front, const glm::vec3& up);
  void ProcessMouse(glm::vec3& front);

  GLFWwindow* window;
  float speed;
  float sensitivity;
  float yaw;
  float pitch;
  double lastMouseX;
  double lastMouseY;
  bool firstMouse;
};
