#include "Texture2D.h"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{
  GLenum ResolveFormat(int channels)
  {
    switch (channels)
    {
      case 1:
        return GL_RED;
      case 3:
        return GL_RGB;
      case 4:
        return GL_RGBA;
      default:
        return GL_NONE;
    }
  }

  void ConfigureTexture2D()
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
}

Texture2D::Texture2D(const std::string& path, bool flipVertically)
  : id(0),
    valid(false)
{
  glGenTextures(1, &id);
  if (id == 0)
  {
    std::cout << "Failed to allocate OpenGL texture for path: " << path << std::endl;
    return;
  }

  glBindTexture(GL_TEXTURE_2D, id);
  ConfigureTexture2D();

  stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

  if (data != nullptr)
  {
    const GLenum format = ResolveFormat(channels);
    if (format != GL_NONE)
    {
      glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format), width, height, 0, format, GL_UNSIGNED_BYTE, data);
      glGenerateMipmap(GL_TEXTURE_2D);
      valid = true;
    }
    else
    {
      std::cout << "Unsupported channel count (" << channels << ") for texture: " << path << std::endl;
    }

    stbi_image_free(data);
  }
  else
  {
    std::cout << "Failed to load texture from path: " << path << ". Using fallback texture." << std::endl;
  }

  if (!valid)
  {
    const unsigned char fallbackPixel[4] = {255, 0, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallbackPixel);
    glGenerateMipmap(GL_TEXTURE_2D);
    valid = true;
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

Texture2D::~Texture2D()
{
  if (id != 0)
  {
    glDeleteTextures(1, &id);
  }
}

void Texture2D::Bind(unsigned int unit) const
{
  if (!valid || id == 0)
  {
    return;
  }

  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, id);
}

bool Texture2D::IsValid() const
{
  return valid;
}

unsigned int Texture2D::GetId() const
{
  return id;
}
