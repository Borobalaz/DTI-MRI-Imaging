#include "Gui/SceneFramebuffer.h"

#include <iostream>

void SceneFramebuffer::EnsureSize(int newWidth, int newHeight)
{
  if (newWidth <= 0 || newHeight <= 0)
  {
    return;
  }

  if (framebuffer != 0 && newWidth == width && newHeight == height)
  {
    return;
  }

  width = newWidth;
  height = newHeight;

  if (framebuffer == 0)
  {
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &colorTexture);
    glGenRenderbuffers(1, &depthStencilRenderbuffer);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  glBindTexture(GL_TEXTURE_2D, colorTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

  glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRenderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRenderbuffer);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    std::cout << "Scene framebuffer is not complete\n";
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneFramebuffer::Destroy()
{
  if (framebuffer == 0)
  {
    return;
  }

  glDeleteFramebuffers(1, &framebuffer);
  glDeleteTextures(1, &colorTexture);
  glDeleteRenderbuffers(1, &depthStencilRenderbuffer);
  framebuffer = 0;
  colorTexture = 0;
  depthStencilRenderbuffer = 0;
  width = 0;
  height = 0;
}

ImTextureID SceneFramebuffer::GetImGuiTextureId() const
{
  return (ImTextureID)(uintptr_t)colorTexture;
}
