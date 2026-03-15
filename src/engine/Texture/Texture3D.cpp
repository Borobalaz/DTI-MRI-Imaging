#include "Texture3D.h"

#include <iostream>

Texture3D::Texture3D(int width,
                     int height,
                     int depth,
                     GLenum internalFormat,
                     GLenum format,
                     GLenum type,
                     const void* data,
                     bool linearFiltering)
  : id(0),
    valid(false)
{
  GLint maxTextureSize = 0;
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maxTextureSize);

  if (width <= 0 || height <= 0 || depth <= 0 ||
      width > maxTextureSize || height > maxTextureSize || depth > maxTextureSize)
  {
    std::cout << "Invalid 3D texture size: " << width << "x" << height << "x" << depth << std::endl;
    return;
  }

  glGenTextures(1, &id);
  if (id == 0)
  {
    std::cout << "Failed to allocate 3D texture." << std::endl;
    return;
  }

  glBindTexture(GL_TEXTURE_3D, id);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, linearFiltering ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, linearFiltering ? GL_LINEAR : GL_NEAREST);

  glTexImage3D(GL_TEXTURE_3D, 0, static_cast<GLint>(internalFormat), width, height, depth, 0, format, type, data);
  glBindTexture(GL_TEXTURE_3D, 0);
  valid = (glGetError() == GL_NO_ERROR);
}

Texture3D::~Texture3D()
{
  if (id != 0)
  {
    glDeleteTextures(1, &id);
  }
}

void Texture3D::Bind(unsigned int unit) const
{
  if (!valid || id == 0)
  {
    return;
  }

  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_3D, id);
}

bool Texture3D::IsValid() const
{
  return valid;
}

unsigned int Texture3D::GetId() const
{
  return id;
}
