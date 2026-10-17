#include "Texture/Texture.h"

// Texture is an abstract interface; concrete implementations live in derived classes.

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