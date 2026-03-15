#include "DirectionalLight.h"

#include <string>

#include "Shader.h"

void DirectionalLight::Apply(Shader& shader) const
{
  if (uniformIndex < 0)
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
