#pragma once

/**
 * @brief PBR material texture slot assignments and constants.
 * 
 * Each slot is bound to a specific texture unit in the fragment shader.
 * Uniforms are accessed as material.texture{Name}Texture in GLSL.
 */

namespace PBR
{
  enum class TextureSlot
  {
    BaseColor = 0,
    Roughness = 1,
    Metallic = 2,
    Normal = 3,
    Emissive = 4,

    Count = 5  // Total number of texture slots
  };

  // Human-readable names for debugging/inspection
  constexpr const char* TextureSlotNames[] = {
    "BaseColor",
    "Roughness",
    "Metallic",
    "Normal",
    "Emissive"
  };

  // Uniform name fragments for GLSL (e.g., "material.baseColorTexture")
  constexpr const char* TextureUniformFragments[] = {
    "albedo",
    "roughness",
    "metallic",
    "normal",
    "emissive"
  };

  // Default property values
  constexpr float DefaultRoughness = 0.5f;
  constexpr float DefaultMetallic = 0.0f;
}
