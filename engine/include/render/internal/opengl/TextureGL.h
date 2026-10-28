#pragma once

#include <glad/glad.h>

#include "render/TextureFormat.h"

// GL-side helpers shared by the texture implementations. Scene-side code works
// with TextureFormat and the Texture interface instead of including this.

struct GLTextureFormat
{
  GLint internalFormat;
  GLenum format;
  GLenum type;
};

GLTextureFormat ToGLTextureFormat(TextureFormat format);

// Resolves the GL pixel format matching an stb_image channel count (1 = red, 3 = rgb, 4 = rgba);
// returns GL_NONE for unsupported counts. Shared by Texture2D and TextureCube image loading.
GLenum ResolveStbImageChannelFormat(int channels);
