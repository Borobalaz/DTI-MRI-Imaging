#include "TextureCube.h"

#include <array>
#include <iostream>

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

  void ConfigureCubeTexture()
  {
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
  }
}

TextureCube::TextureCube(const SkyboxFaces& faces)
  : id(0),
    valid(false)
{
  glGenTextures(1, &id);
  if (id == 0)
  {
    std::cout << "Failed to allocate OpenGL cubemap texture" << std::endl;
    return;
  }

  glBindTexture(GL_TEXTURE_CUBE_MAP, id);
  ConfigureCubeTexture();
  stbi_set_flip_vertically_on_load(0);

  int expectedWidth = -1;
  int expectedHeight = -1;

  valid =
    UploadFace(GL_TEXTURE_CUBE_MAP_POSITIVE_X, "right", faces.right, expectedWidth, expectedHeight) &&
    UploadFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, "left", faces.left, expectedWidth, expectedHeight) &&
    UploadFace(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, "top", faces.top, expectedWidth, expectedHeight) &&
    UploadFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, "bottom", faces.bottom, expectedWidth, expectedHeight) &&
    UploadFace(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, "front", faces.front, expectedWidth, expectedHeight) &&
    UploadFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, "back", faces.back, expectedWidth, expectedHeight);

  if (!valid)
  {
    std::cout << "Cubemap initialization failed. Skybox will be disabled." << std::endl;
  }

  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

TextureCube::~TextureCube()
{
  if (id != 0)
  {
    glDeleteTextures(1, &id);
  }
}

void TextureCube::Bind(unsigned int unit) const
{
  if (!valid || id == 0)
  {
    return;
  }

  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_CUBE_MAP, id);
}

bool TextureCube::IsValid() const
{
  return valid;
}

unsigned int TextureCube::GetId() const
{
  return id;
}

bool TextureCube::UploadFace(unsigned int target,
                             const std::string& faceName,
                             const std::string& path,
                             int& expectedWidth,
                             int& expectedHeight)
{
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

  if (data == nullptr)
  {
    std::cout << "Failed to load cubemap face '" << faceName << "' from path: " << path << std::endl;
    return false;
  }

  const GLenum format = ResolveFormat(channels);
  if (format == GL_NONE)
  {
    std::cout << "Unsupported cubemap face format for '" << faceName << "' at path: " << path << std::endl;
    stbi_image_free(data);
    return false;
  }

  if (expectedWidth == -1 && expectedHeight == -1)
  {
    expectedWidth = width;
    expectedHeight = height;
  }
  else if (width != expectedWidth || height != expectedHeight)
  {
    std::cout << "Cubemap face size mismatch for '" << faceName
              << "'. Expected " << expectedWidth << "x" << expectedHeight
              << " but got " << width << "x" << height
              << " from path: " << path << std::endl;
    stbi_image_free(data);
    return false;
  }

  glTexImage2D(target, 0, static_cast<GLint>(format), width, height, 0, format, GL_UNSIGNED_BYTE, data);
  stbi_image_free(data);
  return true;
}