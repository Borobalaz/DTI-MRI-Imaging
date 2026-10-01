#pragma once

#include <memory>
#include <array>
#include <string>

#include <glm/glm.hpp>

#include "Shader.h"
#include "Texture/Texture.h"
#include "Uniform/UniformProvider.h"
#include "Material/PBRTextureSlots.h"

/**
 * @brief PBR material with Cook-Torrance BRDF lighting model.
 * 
 * Implements UniformProvider to integrate with the uniform binding pipeline.
 * Material properties are applied to shaders via Apply(Shader&).
 * 
 * Texture slots:
 * - 0: baseColor (sRGB, 3-channel or 4-channel with alpha)
 * - 1: roughness (linear, 1-channel grayscale)
 * - 2: metallic (linear, 1-channel grayscale)
 * - 3: normal (tangent-space normals, 3-channel)
 * - 4: emissive (linear, 3-channel)
 */
class Material : public UniformProvider
{
public:
  Material();
  explicit Material(std::shared_ptr<Shader> shader);

  void SetShader(std::shared_ptr<Shader> shader);

  // PBR property setters
  void SetBaseColor(const glm::vec3& color);
  void SetRoughness(float value);
  void SetMetallic(float value);
  void SetEmissive(const glm::vec3& color);

  // Texture setters by slot
  void SetTexture(PBR::TextureSlot slot, std::shared_ptr<Texture> texture);
  bool SetTextureFromFile(PBR::TextureSlot slot,
                          const std::string& texturePath,
                          bool flipVertically = true);

  // Convenience helpers for Scene-level material declaration
  bool SetAlbedoTextureFromFile(const std::string& texturePath, bool flipVertically = true);
  bool SetRoughnessTextureFromFile(const std::string& texturePath, bool flipVertically = true);
  bool SetMetallicTextureFromFile(const std::string& texturePath, bool flipVertically = true);
  bool SetNormalTextureFromFile(const std::string& texturePath, bool flipVertically = true);
  bool SetEmissiveTextureFromFile(const std::string& texturePath, bool flipVertically = true);

  Shader& GetShader() const;

  // UniformProvider implementation
  void Apply(Shader& shader) const override;

private:
  std::shared_ptr<Shader> shader;

  // PBR properties
  glm::vec3 baseColor;
  float roughness;
  float metallic;
  glm::vec3 emissive;

  // PBR textures
  std::array<std::shared_ptr<Texture>, static_cast<size_t>(PBR::TextureSlot::Count)> textures;

  // Helper: bind texture to slot and set uniform flag
  void BindTextureSlot(PBR::TextureSlot slot, Shader& shader) const;
};