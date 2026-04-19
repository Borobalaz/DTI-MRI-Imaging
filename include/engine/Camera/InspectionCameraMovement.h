#pragma once

#include <glm/glm.hpp>

#include "Camera/BaseMovement.h"
#include "Input/InputState.h"

class InspectionCameraMovement : public BaseMovement
{
public:
  InspectionCameraMovement(const glm::vec3& lookAtPoint = glm::vec3(0.0f),
                           float panSpeed = 2.0f,
                           float orbitSensitivity = 0.2f,
                           float zoomSensitivity = 0.8f,
                           float minDistance = 0.2f,
                           float maxDistance = 100.0f);

  void Update(float deltaTime,
              glm::vec3& position,
              glm::vec3& front,
              glm::vec3& up) override;

  void SetInputState(const InputState* state);
  void SetInputEnabled(bool enabled);
  void SetLookAtPoint(const glm::vec3& point);

private:
  void InitializeFromPosition(const glm::vec3& position);

  glm::vec3 lookAtPoint;
  float panSpeed;
  float orbitSensitivity;
  float zoomSensitivity;
  float minDistance;
  float maxDistance;

  float yaw;
  float pitch;
  float distance;
  const InputState* inputState;

  glm::vec2 lastMousePosition;
  bool firstOrbitDrag;
  bool firstPanDrag;
  bool initialized;
  bool inputEnabled;
};
