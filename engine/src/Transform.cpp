#include "Transform.h"

Transform::Transform(const glm::vec3& positionValue,
                     const glm::vec3& orientationValue,
                     const glm::vec3& scaleValue)
  : position(positionValue), orientation(orientationValue), scale(scaleValue)
{
}

glm::mat4 Transform::GetModelMatrix() const
{
  glm::mat4 model = glm::mat4(1.0f);
  model = glm::translate(model, position);
  model = glm::rotate(model, orientation.x, glm::vec3(1.0f, 0.0f, 0.0f));
  model = glm::rotate(model, orientation.y, glm::vec3(0.0f, 1.0f, 0.0f));
  model = glm::rotate(model, orientation.z, glm::vec3(0.0f, 0.0f, 1.0f));
  model = glm::scale(model, scale);
  return model;
}

std::vector<InspectFieldPtr> Transform::GetInspectFields()
{
  auto positionField = MakeInspectField("position", "Position", "Transform", InspectFieldType::Vector3, position,
    [this]() -> InspectValue { return position; },
    [this](const InspectValue& value)
  {
    if (const auto *vector = std::get_if<glm::vec3>(&value)) position = *vector;
  });
  auto orientationField = MakeInspectField("orientation", "Orientation", "Transform", InspectFieldType::Vector3, orientation,
    [this]() -> InspectValue { return orientation; },
    [this](const InspectValue& value)
  {
    if (const auto *vector = std::get_if<glm::vec3>(&value)) orientation = *vector;
  });
  auto scaleField = MakeInspectField("scale", "Scale", "Transform", InspectFieldType::Vector3, scale,
    [this]() -> InspectValue { return scale; },
    [this](const InspectValue& value)
  {
    if (const auto *vector = std::get_if<glm::vec3>(&value)) scale = *vector;
  });
  return {positionField, orientationField, scaleField};
}