#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Shader.h"
#include "Texture.h"

class Material
{
public:
  Material();
  Material(std::shared_ptr<Shader> shader);

  void SetShader(std::shared_ptr<Shader> shader);
  void SetDiffuseTexture(std::shared_ptr<Texture> texture);
  void SetSpecularTexture(std::shared_ptr<Texture> texture);
  void SetTexture(std::shared_ptr<Texture> texture);

  void SetAmbientColor(const glm::vec3& color);
  void SetDiffuseColor(const glm::vec3& color);
  void SetSpecularColor(const glm::vec3& color);
  void SetShininess(float value);

  Shader& GetShader() const;

  void Bind() const;

private:
  std::shared_ptr<Shader> shader;
  std::shared_ptr<Texture> diffuseTexture;
  std::shared_ptr<Texture> specularTexture;

  glm::vec3 ambientColor;
  glm::vec3 diffuseColor;
  glm::vec3 specularColor;
  float shininess;
};