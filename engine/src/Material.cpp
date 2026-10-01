#include "Material.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <filesystem>

#include "Texture/Texture2D.h"

Material::Material()
  : baseColor(1.0f, 1.0f, 1.0f),
    roughness(PBR::DefaultRoughness),
    metallic(PBR::DefaultMetallic),
    emissive(0.0f, 0.0f, 0.0f),
    textures{}
{
}

Material::Material(std::shared_ptr<Shader> shader)
  : shader(std::move(shader)),
    baseColor(1.0f, 1.0f, 1.0f),
    roughness(PBR::DefaultRoughness),
    metallic(PBR::DefaultMetallic),
    emissive(0.0f, 0.0f, 0.0f),
    textures{}
{
}

void Material::SetShader(std::shared_ptr<Shader> shader)
{
  this->shader = shader;
}

void Material::SetBaseColor(const glm::vec3& color)
{
  baseColor = color;
}

void Material::SetRoughness(float value)
{
  roughness = std::clamp(value, 0.0f, 1.0f);
}

void Material::SetMetallic(float value)
{
  metallic = std::clamp(value, 0.0f, 1.0f);
}

void Material::SetEmissive(const glm::vec3& color)
{
  emissive = glm::clamp(color, 0.0f, 1.0f);
}

void Material::SetTexture(PBR::TextureSlot slot, std::shared_ptr<Texture> texture)
{
  size_t slotIndex = static_cast<size_t>(slot);
  if (slotIndex < textures.size())
  {
    textures[slotIndex] = std::move(texture);
  }
}

bool Material::SetTextureFromFile(PBR::TextureSlot slot,
                                  const std::string& texturePath,
                                  bool flipVertically)
{
  if (texturePath.empty())
  {
    return false;
  }

  if (!std::filesystem::exists(texturePath))
  {
    std::cout << "Material texture path does not exist: " << texturePath << std::endl;
    return false;
  }

  std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>(texturePath, flipVertically);
  if (!texture || !texture->IsValid())
  {
    std::cout << "Failed to create texture for material from path: " << texturePath << std::endl;
    return false;
  }

  SetTexture(slot, texture);
  return true;
}

bool Material::SetAlbedoTextureFromFile(const std::string& texturePath, bool flipVertically)
{
  return SetTextureFromFile(PBR::TextureSlot::BaseColor, texturePath, flipVertically);
}

bool Material::SetRoughnessTextureFromFile(const std::string& texturePath, bool flipVertically)
{
  return SetTextureFromFile(PBR::TextureSlot::Roughness, texturePath, flipVertically);
}

bool Material::SetMetallicTextureFromFile(const std::string& texturePath, bool flipVertically)
{
  return SetTextureFromFile(PBR::TextureSlot::Metallic, texturePath, flipVertically);
}

bool Material::SetNormalTextureFromFile(const std::string& texturePath, bool flipVertically)
{
  return SetTextureFromFile(PBR::TextureSlot::Normal, texturePath, flipVertically);
}

bool Material::SetEmissiveTextureFromFile(const std::string& texturePath, bool flipVertically)
{
  return SetTextureFromFile(PBR::TextureSlot::Emissive, texturePath, flipVertically);
}

Shader& Material::GetShader() const
{
  return *shader;
}

void Material::BindTextureSlot(PBR::TextureSlot slot, Shader& shader) const
{
  size_t slotIndex = static_cast<size_t>(slot);
  if (slotIndex >= textures.size())
  {
    return;
  }

  const auto& texture = textures[slotIndex];
  const int textureUnit = static_cast<int>(slot);
  const std::string& uniformFragment = PBR::TextureUniformFragments[slotIndex];

  // Bind texture to unit
  if (texture)
  {
    texture->Bind(textureUnit);
  }

  // Set texture uniform
  std::string textureUniformName = "material." + uniformFragment;
  shader.SetTexture(textureUniformName, textureUnit);

  // Set has-texture flag for shader branching
  std::string hasTextureUniformName = "material.has" + uniformFragment;
  // Capitalize first letter for flag naming: "hasBaseColorTexture"
  hasTextureUniformName[sizeof("material.has") - 1] = static_cast<char>(std::toupper(uniformFragment[0]));
  shader.SetBool(hasTextureUniformName, texture != nullptr);
}

void Material::Apply(Shader& shader) const
{
  if (!this->shader)
  {
    return;
  }

  // Set PBR base properties
  shader.SetVec3("material.albedoFactor", baseColor);
  shader.SetFloat("material.roughnessFactor", roughness);
  shader.SetFloat("material.metallicFactor", metallic);
  shader.SetVec3("material.emissiveFactor", emissive);

  // Bind all texture slots
  for (size_t i = 0; i < static_cast<size_t>(PBR::TextureSlot::Count); ++i)
  {
    BindTextureSlot(static_cast<PBR::TextureSlot>(i), shader);
  }
}