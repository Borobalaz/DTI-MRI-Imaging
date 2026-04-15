#pragma once

#include <mutex>
#include <unordered_set>

#include <QPointF>
#include <Qt>

#include <glm/glm.hpp>

#include "Camera/BaseMovement.h"

class QtInspectionMovement : public BaseMovement
{
public:
  QtInspectionMovement(const glm::vec3 &lookAtPoint = glm::vec3(0.0f),
                       float panSpeed = 2.0f,
                       float orbitSensitivity = 0.2f,
                       float zoomSensitivity = 0.8f,
                       float minDistance = 0.2f,
                       float maxDistance = 100.0f);

  void Update(float deltaTime,
              glm::vec3 &position,
              glm::vec3 &front,
              glm::vec3 &up) override;

  void SetInputEnabled(bool enabled);
  void SetLookAtPoint(const glm::vec3 &point);

  void SetKeyPressed(int key, bool pressed);
  void SetMouseButtonPressed(Qt::MouseButton button, bool pressed);
  void SetMousePosition(const QPointF &position);
  void AddScrollDelta(float delta);

private:
  void InitializeFromPosition(const glm::vec3 &position);

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

  QPointF lastMousePosition;
  bool firstOrbitDrag;
  bool firstPanDrag;
  bool initialized;
  bool inputEnabled;

  bool leftMousePressed;
  bool rightMousePressed;
  bool middleMousePressed;

  float accumulatedOrbitDeltaX;
  float accumulatedOrbitDeltaY;
  float accumulatedPanDeltaX;
  float accumulatedPanDeltaY;

  std::unordered_set<int> pressedKeys;
  mutable std::mutex mutex;
};
