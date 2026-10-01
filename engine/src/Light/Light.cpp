#include "Light/Light.h"

#include <algorithm>


Light::Light(const std::string& id,
             const glm::vec3& ambient,
             const glm::vec3& diffuse,
             const glm::vec3& specular)
  : ambient(ambient),
    diffuse(diffuse),
    specular(specular),
    uniformIndex(-1),
    id(id)
{
}

std::string Light::GetInspectDisplayName() const
{
  return id;
}

std::vector<InspectFieldPtr> Light::GetInspectFields()
{
  auto enabledField = MakeInspectField("enabled", "Enabled", "Light", InspectFieldType::Boolean, enabled,
    [this]() -> InspectValue { return enabled; },
    [this](const InspectValue &value)
  {
    if (const auto *boolean = std::get_if<bool>(&value))
      enabled = *boolean;
  });

  auto intensityField = MakeInspectField("intensity", "Intensity", "Light", InspectFieldType::Number,
    static_cast<double>(intensity),
    [this]() -> InspectValue { return static_cast<double>(intensity); },
    [this](const InspectValue &value)
  {
    if (const auto *number = std::get_if<double>(&value))
      intensity = static_cast<float>(std::max(0.0, *number));
  });
  intensityField->minimum = 0.0;
  intensityField->maximum = 100.0;
  intensityField->step = 0.1;

  auto ambientField = MakeInspectField("ambient", "Ambient", "Color", InspectFieldType::Color, ambient,
    [this]() -> InspectValue { return ambient; },
    [this](const InspectValue &value)
  {
    if (const auto *color = std::get_if<glm::vec3>(&value)) ambient = *color;
  });

  auto diffuseField = MakeInspectField("diffuse", "Diffuse", "Color", InspectFieldType::Color, diffuse,
    [this]() -> InspectValue { return diffuse; },
    [this](const InspectValue &value)
  {
    if (const auto *color = std::get_if<glm::vec3>(&value)) diffuse = *color;
  });

  auto specularField = MakeInspectField("specular", "Specular", "Color", InspectFieldType::Color, specular,
    [this]() -> InspectValue { return specular; },
    [this](const InspectValue &value)
  {
    if (const auto *color = std::get_if<glm::vec3>(&value)) specular = *color;
  });

  return {enabledField, intensityField, ambientField, diffuseField, specularField};
}

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


