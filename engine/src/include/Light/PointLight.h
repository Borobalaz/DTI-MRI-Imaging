#pragma once

#include <glm/glm.hpp>

#include "Light/Light.h"
#include "Transform.h"

class PointLight : public Light
{
public:
  PointLight(const std::string& id,
             const glm::vec3& position,
             const glm::vec3& ambient,
             const glm::vec3& diffuse,
             const glm::vec3& specular,
             float constant,
             float linear,
             float quadratic);

  void Apply(Shader& shader) const override;
  std::vector<InspectFieldPtr> GetInspectFields() override;

  float constant;
  float linear;
  float quadratic;

private:
  Transform transform;
};
