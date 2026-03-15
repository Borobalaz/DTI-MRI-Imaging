#pragma once

#include <glm/glm.hpp>

#include "Light.h"

class DirectionalLight : public Light
{
public:
  DirectionalLight();
  DirectionalLight(const glm::vec3& direction,
                   const glm::vec3& ambient,
                   const glm::vec3& diffuse,
                   const glm::vec3& specular);

  void Apply(Shader& shader) const override;

  glm::vec3 direction;
};

inline DirectionalLight::DirectionalLight()
  : Light(),
    direction(0.0f, -1.0f, 0.0f)
{
}

inline DirectionalLight::DirectionalLight(const glm::vec3& direction,
                                          const glm::vec3& ambient,
                                          const glm::vec3& diffuse,
                                          const glm::vec3& specular)
  : Light(ambient, diffuse, specular),
    direction(direction)
{
}
