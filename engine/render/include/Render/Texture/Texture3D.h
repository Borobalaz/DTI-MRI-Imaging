#pragma once

#include "Render/Texture/Texture.h"
#include "Render/TextureFormat.h"

class Texture3D : public Texture
{
public:
  Texture3D(int width,
            int height,
            int depth,
            TextureFormat format,
            const void* data,
            bool linearFiltering = true);
  ~Texture3D() override;

  bool IsValid() const override;

private:
  void Bind(unsigned int unit) const override;
  unsigned int GetId() const override;

  unsigned int id;
  bool valid;
};
