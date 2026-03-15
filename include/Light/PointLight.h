#pragma once

#include <glm/glm.hpp>

#include "Light.h"

class PointLight : public Light
{
public:
  PointLight();
  PointLight(const glm::vec3& position,
             const glm::vec3& ambient,
             const glm::vec3& diffuse,
             const glm::vec3& specular,
             float constant,
             float linear,
             float quadratic);

  void Apply(Shader& shader) const override;

  glm::vec3 position;
  float constant;
  float linear;
  float quadratic;
};

/**
 * @brief Construct a new Point Light:: Point Light object
 * 
 */
inline PointLight::PointLight()
  : Light(),
    position(0.0f, 0.0f, 0.0f),
    constant(1.0f),
    linear(0.09f),
    quadratic(0.032f)
{
}

/**
 * @brief Construct a new Point Light:: Point Light object
 * 
 * @param position 
 * @param ambient 
 * @param diffuse 
 * @param specular 
 * @param constant 
 * @param linear 
 * @param quadratic 
 */
inline PointLight::PointLight(const glm::vec3& position,
                              const glm::vec3& ambient,
                              const glm::vec3& diffuse,
                              const glm::vec3& specular,
                              float constant,
                              float linear,
                              float quadratic)
  : Light(ambient, diffuse, specular),
    position(position),
    constant(constant),
    linear(linear),
    quadratic(quadratic)
{
}
