#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Render/Texture/Texture3D.h"
#include "Render/Uniform/IUniformSink.h"

class VolumeTextureSet
{
public:
  void AddTexture(std::shared_ptr<Texture3D> texture);
  void Bind(IUniformSink &shader, const std::string &uniformBaseName) const;
  void Bind(IUniformSink &shader, int textureIndex, const std::string &uniformBaseName, const std::string &uniformSubName) const;
  bool IsValid() const;
  size_t Size() const;

private:
  std::vector<std::shared_ptr<Texture3D>> textures;

  static const unsigned int kVolumeTextureBaseUnit = 8;
};
