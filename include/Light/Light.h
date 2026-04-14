#pragma once

#include <glm/glm.hpp>

#include "Gui/Inspectable.h"
#include "UniformProvider.h"

class Light : public UniformProvider, public IInspectable
{
public:
  enum class Type
  {
    Point = 0,
    Directional = 1
  };

  Light();
  Light(const glm::vec3& ambient,
        const glm::vec3& diffuse,
        const glm::vec3& specular);
  virtual ~Light() = default;

  void SetUniformIndex(int index);
  int GetUniformIndex() const;
  void Apply(Shader& shader) const override;
  void CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix) override;  
  void CollectInspectableNodes(std::vector<InspectableNode>& out)
  {
    // Lights do not have nested nodes in this implementation
  }

  void SetEnabled(bool isEnabled) { enabled = isEnabled; }
  bool GetEnabled() const { return enabled; }

  glm::vec3 ambient;
  glm::vec3 diffuse;
  glm::vec3 specular;

protected:
  int uniformIndex;
  bool enabled = true;
};

inline Light::Light()
  : ambient(0.05f, 0.05f, 0.05f),
    diffuse(1.0f, 1.0f, 1.0f),
    specular(1.0f, 1.0f, 1.0f),
    uniformIndex(-1)
{
}

inline Light::Light(const glm::vec3& ambient,
                    const glm::vec3& diffuse,
                    const glm::vec3& specular)
  : ambient(ambient),
    diffuse(diffuse),
  specular(specular),
  uniformIndex(-1)
{
}
