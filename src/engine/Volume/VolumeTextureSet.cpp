#include "VolumeTextureSet.h"

/**
 * @brief Push back a new texture into the set. The texture must be already initialized and valid.
 * 
 * @param texture 
 */
void VolumeTextureSet::AddTexture(std::shared_ptr<Texture3D> texture)
{
  if (texture)
  {
    textures.push_back(std::move(texture));
  }
}

/**
 * @brief Uplaod the textures in this set to the GPU and bind them to the shader's uniform samplers. 
 *        The shader must have sampler uniforms named like "uniformBaseName[0]", "uniformBaseName[1]", etc. for each texture in the set.
 * 
 * @param shader 
 * @param baseUnit 
 * @param uniformBaseName 
 */
void VolumeTextureSet::Bind(Shader& shader, const std::string& uniformBaseName) const
{
  if (textures.empty())
  {
    return;
  }

  for (size_t i = 0; i < textures.size(); ++i)
  {
    if (!textures[i])
    {
      continue;
    }

    const std::string uniformName = uniformBaseName + "[" + std::to_string(i) + "]";
    textures[i]->Bind(kVolumeTextureBaseUnit + static_cast<unsigned int>(i));
    shader.SetTexture(uniformName, static_cast<int>(kVolumeTextureBaseUnit + static_cast<unsigned int>(i)));
  }
}

/**
 * @brief Upload a specific texture in the set to the GPU and bind it to a shader uniform sampler.
 *        The shader must have a sampler uniform named like "uniformBaseName.uniformSubName".
 * 
 * @param shader 
 * @param textureIndex 
 * @param uniformBaseName 
 * @param uniformSubName 
 */
void VolumeTextureSet::Bind(Shader& shader, int textureIndex, const std::string& uniformBaseName, const std::string& uniformSubName) const
{
  if (textureIndex < 0 || static_cast<size_t>(textureIndex) >= textures.size())
  {
    return;
  }

  if (!textures[textureIndex])
  {
    return;
  }

  const std::string uniformName = uniformBaseName + "." + uniformSubName;
  if (!shader.HasUniform(uniformName))
  {
    return;
  }

  textures[textureIndex]->Bind(kVolumeTextureBaseUnit + static_cast<unsigned int>(textureIndex));
  shader.SetTexture(uniformName, static_cast<int>(kVolumeTextureBaseUnit + static_cast<unsigned int>(textureIndex)));
}

/**
 * @brief Check if the texture set is valid by ensuring it has at least one texture and all textures are valid.
 * 
 * @return true 
 * @return false 
 */
bool VolumeTextureSet::IsValid() const
{
  if (textures.empty())
  {
    return false;
  }

  for (const auto& texture : textures)
  {
    if (!texture || !texture->IsValid())
    {
      return false;
    }
  }

  return true;
}

/**
 * @brief Get the number of textures in the set.
 * 
 * @return size_t 
 */
size_t VolumeTextureSet::Size() const
{
  return textures.size();
}
