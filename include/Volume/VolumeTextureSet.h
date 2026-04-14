#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Shader.h"
#include "Texture3D.h"

class VolumeTextureSet
{
public:
  void AddTexture(std::shared_ptr<Texture3D> texture);
  void Bind(Shader &shader, const std::string &uniformBaseName) const;
  void VolumeTextureSet::Bind(Shader &shader, int textureIndex, const std::string &uniformBaseName, const std::string &uniformSubName) const;
  bool IsValid() const;
  size_t Size() const;

private:
  std::vector<std::shared_ptr<Texture3D>> textures;

  static const unsigned int kVolumeTextureBaseUnit = 8;
};
