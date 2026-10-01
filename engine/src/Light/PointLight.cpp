#include "Light/PointLight.h"

#include <cmath>
#include <string>

#include "Shader.h"

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
PointLight::PointLight(const std::string& id,
                              const glm::vec3& position,
                              const glm::vec3& ambient,
                              const glm::vec3& diffuse,
                              const glm::vec3& specular,
                              float constant,
                              float linear,
                              float quadratic)
  : Light(id, ambient, diffuse, specular),
    constant(constant),
    linear(linear),
    quadratic(quadratic),
    transform(position, glm::vec3(0.0f), glm::vec3(1.0f))
{
}

/**
 * @brief 
 * 
 * @param shader 
 */
void PointLight::Apply(Shader& shader) const
{
  if (uniformIndex < 0 || !enabled)
  {
    return;
  }

  const std::string prefix = "lights[" + std::to_string(uniformIndex) + "]";

  shader.SetInt(prefix + ".type", static_cast<int>(Light::Type::Point));
  shader.SetVec3(prefix + ".ambient", ambient);
  shader.SetVec3(prefix + ".diffuse", diffuse);
  shader.SetVec3(prefix + ".specular", specular);
  shader.SetFloat(prefix + ".intensity", intensity);
  shader.SetVec3(prefix + ".position", transform.GetPosition());
  shader.SetVec3(prefix + ".direction", glm::vec3(0.0f, -1.0f, 0.0f));
  shader.SetFloat(prefix + ".constant", constant);
  shader.SetFloat(prefix + ".linear", linear);
  shader.SetFloat(prefix + ".quadratic", quadratic);
}

std::vector<InspectFieldPtr> PointLight::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields = transform.GetInspectFields();
  const std::vector<InspectFieldPtr> lightFields = Light::GetInspectFields();
  fields.insert(fields.end(), lightFields.begin(), lightFields.end());
  return fields;
}