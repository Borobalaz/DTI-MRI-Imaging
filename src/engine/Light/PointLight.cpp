#include "PointLight.h"

#include <string>

#include "Shader.h"

void PointLight::Apply(Shader& shader) const
{
  if (uniformIndex < 0)
  {
    return;
  }

  const std::string prefix = "lights[" + std::to_string(uniformIndex) + "]";

  shader.SetInt(prefix + ".type", static_cast<int>(Light::Type::Point));
  shader.SetVec3(prefix + ".ambient", ambient);
  shader.SetVec3(prefix + ".diffuse", diffuse);
  shader.SetVec3(prefix + ".specular", specular);
  shader.SetVec3(prefix + ".position", position);
  shader.SetVec3(prefix + ".direction", glm::vec3(0.0f, -1.0f, 0.0f));
  shader.SetFloat(prefix + ".constant", constant);
  shader.SetFloat(prefix + ".linear", linear);
  shader.SetFloat(prefix + ".quadratic", quadratic);
}
