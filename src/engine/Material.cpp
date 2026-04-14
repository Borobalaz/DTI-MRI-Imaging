#include "Material.h"

#include <algorithm>

Material::Material()
  : ambientColor(0.2f, 0.2f, 0.2f),
    diffuseColor(0.8f, 0.8f, 0.8f),
    specularColor(1.0f, 1.0f, 1.0f),
    shininess(32.0f)
{
}

Material::Material(std::shared_ptr<Shader> shader)
  : shader(std::move(shader)),
    ambientColor(0.2f, 0.2f, 0.2f),
    diffuseColor(0.8f, 0.8f, 0.8f),
    specularColor(1.0f, 1.0f, 1.0f),
    shininess(32.0f)
{
}

void Material::SetShader(std::shared_ptr<Shader> shader)
{
  this->shader = shader;
}

void Material::SetDiffuseTexture(std::shared_ptr<Texture> texture)
{
  diffuseTexture = std::move(texture);
}

void Material::SetSpecularTexture(std::shared_ptr<Texture> texture)
{
  specularTexture = std::move(texture);
}

void Material::SetTexture(std::shared_ptr<Texture> texture)
{
  SetDiffuseTexture(std::move(texture));
}

void Material::SetAmbientColor(const glm::vec3& color)
{
  ambientColor = color;
}

void Material::SetDiffuseColor(const glm::vec3& color)
{
  diffuseColor = color;
}

void Material::SetSpecularColor(const glm::vec3& color)
{
  specularColor = color;
}

void Material::SetShininess(float value)
{
  shininess = std::clamp(value, 0.0f, 256.0f);
}

Shader& Material::GetShader() const
{
  return *shader;
}

void Material::Bind() const
{
  if (!shader)
  {
    return;
  }

  shader->Use();

  if (shader->HasUniform("material.ambientColor"))
  {
    shader->SetVec3("material.ambientColor", ambientColor);
  }
  if (shader->HasUniform("material.diffuseColor"))
  {
    shader->SetVec3("material.diffuseColor", diffuseColor);
  }
  if (shader->HasUniform("material.specularColor"))
  {
    shader->SetVec3("material.specularColor", specularColor);
  }
  if (shader->HasUniform("material.shininess"))
  {
    shader->SetFloat("material.shininess", shininess);
  }

  constexpr int diffuseTextureUnit = 0;
  constexpr int specularTextureUnit = 1;

  if (diffuseTexture)
  {
    diffuseTexture->Bind(diffuseTextureUnit);
    if (shader->HasUniform("material.diffuseTexture"))
    {
      shader->SetTexture("material.diffuseTexture", diffuseTextureUnit);
    }
  }
  if (shader->HasUniform("material.hasDiffuseTexture"))
  {
    shader->SetBool("material.hasDiffuseTexture", diffuseTexture != nullptr);
  }

  if (specularTexture)
  {
    specularTexture->Bind(specularTextureUnit);
    if (shader->HasUniform("material.specularTexture"))
    {
      shader->SetTexture("material.specularTexture", specularTextureUnit);
    }
  }
  if (shader->HasUniform("material.hasSpecularTexture"))
  {
    shader->SetBool("material.hasSpecularTexture", specularTexture != nullptr);
  }
}