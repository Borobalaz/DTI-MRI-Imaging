#include "DirectionalLight.h"

#include <string>

#include "Shader.h"

void DirectionalLight::Apply(Shader& shader) const
{
  if (uniformIndex < 0 || !enabled)
  {
    return;
  }

  if (!shader.HasUniform("lightCount") || !shader.HasUniform("lights[0].type"))
  {
    return;
  }

  const std::string prefix = "lights[" + std::to_string(uniformIndex) + "]";

  shader.SetInt(prefix + ".type", static_cast<int>(Light::Type::Directional));
  shader.SetVec3(prefix + ".ambient", ambient);
  shader.SetVec3(prefix + ".diffuse", diffuse);
  shader.SetVec3(prefix + ".specular", specular);
  shader.SetVec3(prefix + ".position", glm::vec3(0.0f));
  shader.SetVec3(prefix + ".direction", direction);
  shader.SetFloat(prefix + ".constant", 1.0f);
  shader.SetFloat(prefix + ".linear", 0.0f);
  shader.SetFloat(prefix + ".quadratic", 0.0f);
}

void DirectionalLight::CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix)
{
  const std::string group = groupPrefix.empty() ? "DirectionalLight" : groupPrefix;

  UiField directionField;
  directionField.group = group;
  directionField.label = "Direction";
  directionField.kind = UiFieldKind::Vec3;
  directionField.speed = 0.01f;
  directionField.getter = [this]() -> UiFieldValue
  {
    return direction;
  };
  directionField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<glm::vec3>(value))
    {
      return;
    }

    direction = std::get<glm::vec3>(value);
  };
  out.push_back(std::move(directionField));

  // Collect base Light fields
  Light::CollectInspectableFields(out, group);
}