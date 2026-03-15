#pragma once

#include "Texture.h"

class Texture3D : public Texture
{
public:
  Texture3D(int width,
            int height,
            int depth,
            GLenum internalFormat,
            GLenum format,
            GLenum type,
            const void* data,
            bool linearFiltering = true);
  ~Texture3D() override;

  void Bind(unsigned int unit) const override;
  bool IsValid() const override;
  unsigned int GetId() const override;

private:
  unsigned int id;
  bool valid;
};
