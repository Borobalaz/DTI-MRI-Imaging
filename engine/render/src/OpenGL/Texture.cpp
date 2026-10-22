#include "Render/Texture/Texture.h"

#include "OpenGL/TextureGL.h"

// Texture is an abstract interface; concrete implementations live in derived classes.

GLTextureFormat ToGLTextureFormat(TextureFormat format)
{
  switch (format)
  {
    case TextureFormat::R32F:
      return {GL_R32F, GL_RED, GL_FLOAT};
    case TextureFormat::RGB32F:
      return {GL_RGB32F, GL_RGB, GL_FLOAT};
    case TextureFormat::RGBA32F:
      return {GL_RGBA32F, GL_RGBA, GL_FLOAT};
    case TextureFormat::RGBA8:
      return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case TextureFormat::Depth24:
      return {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT};
  }

  // Unreachable for valid enum values; keeps compilers that cannot prove exhaustiveness quiet.
  return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
}

GLenum ResolveStbImageChannelFormat(int channels)
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