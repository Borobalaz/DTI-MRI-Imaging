#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "UniformProvider.h"
#include "BaseMovement.h"

class Camera : public UniformProvider
{
public:
  Camera();
  virtual ~Camera();

  virtual glm::mat4 GetViewMatrix() const = 0;
  virtual glm::mat4 GetProjectionMatrix() const = 0;
  void Apply(Shader& shader) const override;

  virtual void Update(float deltaTime) = 0;

  void SetPosition(const glm::vec3& pos);
  glm::vec3 GetPosition() const;
  void SetAspect(float newAspect);
  float GetAspect() const;

  void SetMoveComponent(std::unique_ptr<BaseMovement> component);

protected:
  void Move(float deltaTime);

  glm::vec3 position;
  glm::vec3 front;
  glm::vec3 up;
  float aspect;

private:
  std::unique_ptr<BaseMovement> moveComponent;
};