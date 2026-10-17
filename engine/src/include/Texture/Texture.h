#pragma once

#include <glad/glad.h>

class Texture
{
public:
  virtual ~Texture() = default;

  virtual void Bind(unsigned int unit) const = 0;
  virtual bool IsValid() const = 0;
  virtual unsigned int GetId() const = 0;
};

// Resolves the GL pixel format matching an stb_image channel count (1 = red, 3 = rgb, 4 = rgba);
// returns GL_NONE for unsupported counts. Shared by Texture2D and TextureCube image loading.
GLenum ResolveStbImageChannelFormat(int channels);