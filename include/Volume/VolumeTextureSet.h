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
  void Bind(Shader& shader, unsigned int baseUnit, const std::string& uniformBaseName) const;
  bool IsValid() const;
  size_t Size() const;

private:
  std::vector<std::shared_ptr<Texture3D>> textures;
};
