#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Inspection/InspectField.h"

class Transform : public InspectProvider
{
public:
  Transform() = default;

  Transform(const glm::vec3& positionValue,
            const glm::vec3& orientationValue,
            const glm::vec3& scaleValue);

  const glm::vec3& GetPosition() const { return position; }
  const glm::vec3& GetOrientation() const { return orientation; }
  const glm::vec3& GetScale() const { return scale; }

  const glm::vec3& GetRotation() const { return orientation; }

  void SetPosition(const glm::vec3& value) { position = value; }
  void SetOrientation(const glm::vec3& value) { orientation = value; }
  void SetScale(const glm::vec3& value) { scale = value; }

  void SetRotation(const glm::vec3& value) { orientation = value; }

  glm::mat4 GetModelMatrix() const;

  std::string GetInspectDisplayName() const override { return "Transform"; }

  std::vector<InspectFieldPtr> GetInspectFields() override;

  bool HasVisibility() const override { return false; }
  bool IsVisible() const override { return true; }

  std::optional<float> CastRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const override
  {
    (void)rayOrigin;
    (void)rayDirection;
    return std::nullopt;
  }

private:
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 orientation{0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};
};
