#include "Light/DirectionalLight.h"

#include <string>

#include "Shader.h"

DirectionalLight::DirectionalLight(const std::string& id,
                                   const glm::vec3& direction,
                                   const glm::vec3& ambient,
                                   const glm::vec3& diffuse,
                                   const glm::vec3& specular)
  : Light(id, ambient, diffuse, specular),
    direction(direction)
{
}

void DirectionalLight::Apply(Shader& shader) const
{
  if (uniformIndex < 0 || !enabled)
  {
    return;
  }

  const std::string prefix = "lights[" + std::to_string(uniformIndex) + "]";

  shader.SetInt(prefix + ".type", static_cast<int>(Light::Type::Directional));
  shader.SetVec3(prefix + ".ambient", ambient);
  shader.SetVec3(prefix + ".diffuse", diffuse);
  shader.SetVec3(prefix + ".specular", specular);
  shader.SetFloat(prefix + ".intensity", intensity);
  shader.SetVec3(prefix + ".position", glm::vec3(0.0f));
  shader.SetVec3(prefix + ".direction", direction);
  shader.SetFloat(prefix + ".constant", 1.0f);
  shader.SetFloat(prefix + ".linear", 0.0f);
  shader.SetFloat(prefix + ".quadratic", 0.0f);
}