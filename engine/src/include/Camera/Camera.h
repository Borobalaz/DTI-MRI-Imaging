#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Uniform/UniformProvider.h"
#include "Camera/BaseMovement.h"
#include "Transform.h"
#include "Inspection/InspectField.h"

class Camera : public UniformProvider, public InspectProvider
{
public:
  Camera();
  virtual ~Camera();

  virtual glm::mat4 GetViewMatrix() const = 0;
  virtual glm::mat4 GetProjectionMatrix() const = 0;
  void Apply(Shader& shader) const override;
  std::string GetInspectDisplayName() const override;
  std::vector<InspectFieldPtr> GetInspectFields() override;

  virtual void Update(float deltaTime) = 0;

  void SetPosition(const glm::vec3& pos);
  glm::vec3 GetPosition() const;
  void SetAspect(float newAspect);
  float GetAspect() const;
  void SetFocalDistance(float newFocalDistance);
  float GetFocalDistance() const;
  void SetFocalSize(float newFocalSize);
  float GetFocalSize() const;

  void SetMoveComponent(std::unique_ptr<BaseMovement> component);

protected:
  void Move(float deltaTime);
  void NotifyMovementCameraStateChanged();
  glm::vec3 GetForwardVector() const;

  Transform transform;
  glm::vec3 front;
  glm::vec3 up;
  float aspect;
  float focalDistance;
  float focalSize;

private:
  std::unique_ptr<BaseMovement> moveComponent;
};