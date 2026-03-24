#include "Light.h"

void Light::SetUniformIndex(int index)
{
  uniformIndex = index;
}

int Light::GetUniformIndex() const
{
  return uniformIndex;
}

void Light::Apply(Shader& shader) const
{
  // Base Light class doesn't apply any uniforms
  // Derived classes (PointLight, DirectionalLight) override this method
}

void Light::CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix)
{
  const std::string group = groupPrefix.empty() ? "Light" : groupPrefix;

  UiField enabledField;
  enabledField.group = group;
  enabledField.label = "Enabled";
  enabledField.kind = UiFieldKind::Bool;
  enabledField.getter = [this]() -> UiFieldValue
  {
    return enabled;
  };
  enabledField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<bool>(value))
    {
      return;
    }

    enabled = std::get<bool>(value);
  };
  out.push_back(std::move(enabledField));

  UiField ambientField;
  ambientField.group = group;
  ambientField.label = "Ambient";
  ambientField.kind = UiFieldKind::Color3;
  ambientField.speed = 0.01f;
  ambientField.getter = [this]() -> UiFieldValue
  {
    return ambient;
  };
  ambientField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<glm::vec3>(value))
    {
      return;
    }

    ambient = std::get<glm::vec3>(value);
  };
  out.push_back(std::move(ambientField));

  UiField diffuseField;
  diffuseField.group = group;
  diffuseField.label = "Diffuse";
  diffuseField.kind = UiFieldKind::Color3;
  diffuseField.speed = 0.01f;
  diffuseField.getter = [this]() -> UiFieldValue
  {
    return diffuse;
  };
  diffuseField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<glm::vec3>(value))
    {
      return;
    }

    diffuse = std::get<glm::vec3>(value);
  };
  out.push_back(std::move(diffuseField));

  UiField specularField;
  specularField.group = group;
  specularField.label = "Specular";
  specularField.kind = UiFieldKind::Color3;
  specularField.speed = 0.01f;
  specularField.getter = [this]() -> UiFieldValue
  {
    return specular;
  };
  specularField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<glm::vec3>(value))
    {
      return;
    }

    specular = std::get<glm::vec3>(value);
  };
  out.push_back(std::move(specularField));
}

