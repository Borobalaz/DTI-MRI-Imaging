#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "BaseMovement.h"

class InspectionMovement : public BaseMovement
{
public:
  InspectionMovement(
    GLFWwindow* window,
    const glm::vec3& lookAtPoint = glm::vec3(0.0f),
    float panSpeed = 2.0f,
    float orbitSensitivity = 0.2f,
    float zoomSensitivity = 0.8f,
    float minDistance = 0.2f,
    float maxDistance = 100.0f
  );

  ~InspectionMovement() override;

  void Update(
    float deltaTime,
    glm::vec3& position,
    glm::vec3& front,
    glm::vec3& up
  ) override;

  void SetLookAtPoint(const glm::vec3& point);
  glm::vec3 GetLookAtPoint() const;
  void SetInputEnabled(bool enabled);

private:
  static void ScrollCallbackRouter(GLFWwindow* window, double xOffset, double yOffset);

  void InitializeFromPosition(const glm::vec3& position);
  void ProcessOrbit();
  void ProcessMousePan();
  void ProcessPan(float deltaTime, const glm::vec3& front, const glm::vec3& up);
  void ProcessZoom();

  GLFWwindow* window;
  GLFWscrollfun previousScrollCallback;

  glm::vec3 lookAtPoint;
  float panSpeed;
  float orbitSensitivity;
  float zoomSensitivity;
  float minDistance;
  float maxDistance;

  float yaw;
  float pitch;
  float distance;
  float pendingScrollOffset;

  double lastMouseX;
  double lastMouseY;
  bool firstDrag;
  bool firstPanDrag;
  bool initialized;
  bool inputEnabled;
};