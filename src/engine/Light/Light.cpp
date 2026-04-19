#include "Light/Light.h"

#include "ui/widgets/inspect_fields/InspectCheckboxFieldWidget.h"
#include "ui/widgets/inspect_fields/InspectColorFieldWidget.h"

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

std::vector<std::shared_ptr<IInspectWidget>> Light::GetInspectFields()
{
  auto enabledField = std::make_shared<InspectCheckboxFieldWidget>("enabled", "Enabled", "Light");
  enabledField->SetValue(enabled);
  enabledField->valueChangedCallback = [this](const QVariant &value)
  {
    enabled = value.toBool();
  };

  auto ambientField = std::make_shared<InspectColorFieldWidget>("ambient", "Ambient", "Color");
  ambientField->SetValue(QVariantList{ambient.r, ambient.g, ambient.b});
  ambientField->valueChangedCallback = [this](const QVariant &value)
  {
    const QVariantList list = value.toList();
    if (list.size() >= 3)
    {
      ambient = glm::vec3(static_cast<float>(list[0].toDouble()),
                          static_cast<float>(list[1].toDouble()),
                          static_cast<float>(list[2].toDouble()));
    }
  };

  auto diffuseField = std::make_shared<InspectColorFieldWidget>("diffuse", "Diffuse", "Color");
  diffuseField->SetValue(QVariantList{diffuse.r, diffuse.g, diffuse.b});
  diffuseField->valueChangedCallback = [this](const QVariant &value)
  {
    const QVariantList list = value.toList();
    if (list.size() >= 3)
    {
      diffuse = glm::vec3(static_cast<float>(list[0].toDouble()),
                          static_cast<float>(list[1].toDouble()),
                          static_cast<float>(list[2].toDouble()));
    }
  };

  auto specularField = std::make_shared<InspectColorFieldWidget>("specular", "Specular", "Color");
  specularField->SetValue(QVariantList{specular.r, specular.g, specular.b});
  specularField->valueChangedCallback = [this](const QVariant &value)
  {
    const QVariantList list = value.toList();
    if (list.size() >= 3)
    {
      specular = glm::vec3(static_cast<float>(list[0].toDouble()),
                           static_cast<float>(list[1].toDouble()),
                           static_cast<float>(list[2].toDouble()));
    }
  };

  return {enabledField, ambientField, diffuseField, specularField};
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


