#pragma once

#include <string>

#include "Texture/Texture.h"

class Texture2D : public Texture
{
public:
  explicit Texture2D(const std::string& path, bool flipVertically = true);

  // Allocates an empty 2D texture (data may be nullptr) for use as a
  // framebuffer attachment, mirroring Texture3D's allocation constructor.
  Texture2D(int width,
            int height,
            GLenum internalFormat,
            GLenum format,
            GLenum type,
            const void* data,
            bool linearFiltering = true);

  ~Texture2D() override;

  void Bind(unsigned int unit) const override;
  bool IsValid() const override;
  unsigned int GetId() const override;

private:
  unsigned int id;
  bool valid;
};
