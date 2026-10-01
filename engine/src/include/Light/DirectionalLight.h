#pragma once

#include <glm/glm.hpp>

#include "Light/Light.h"

class DirectionalLight : public Light
{
public:
  DirectionalLight(const std::string& id,
                   const glm::vec3& direction,
                   const glm::vec3& ambient,
                   const glm::vec3& diffuse,
                   const glm::vec3& specular);

  void Apply(Shader& shader) const override;

  glm::vec3 direction;
};

