#pragma once

#include <string>

#include "render/texture/Texture.h"
#include "render/TextureFormat.h"

class Texture2D : public Texture
{
public:
  explicit Texture2D(const std::string& path, bool flipVertically = true);

  // Allocates an empty 2D texture (data may be nullptr) for use as a
  // framebuffer attachment, mirroring Texture3D's allocation constructor.
  Texture2D(int width,
            int height,
            TextureFormat format,
            const void* data,
            bool linearFiltering = true);

  ~Texture2D() override;

  bool IsValid() const override;

private:
  void Bind(unsigned int unit) const override;
  unsigned int GetId() const override;

  unsigned int id;
  bool valid;
};
