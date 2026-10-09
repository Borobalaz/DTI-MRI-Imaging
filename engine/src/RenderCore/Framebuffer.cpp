#include "RenderCore/Framebuffer.h"

#include <iostream>

Framebuffer::Framebuffer(bool withDepth)
  : withDepth(withDepth)
{
}

Framebuffer::~Framebuffer()
{
  Destroy();
}

void Framebuffer::Allocate(int w, int h)
{
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  colorTexture = std::make_unique<Texture2D>(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, true);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture->GetId(), 0);

  if (withDepth)
  {
    depthTexture = std::make_unique<Texture2D>(w, h, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr, false);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture->GetId(), 0);
  }

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    std::cout << "Framebuffer incomplete (" << w << "x" << h << ")" << std::endl;
  }

  width = w;
  height = h;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Resize(int w, int h)
{
  if (w <= 0 || h <= 0 || (fbo != 0 && w == width && h == height))
  {
    return;
  }

  Destroy();
  Allocate(w, h);
}

void Framebuffer::Bind() const
{
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, width, height);
}

void Framebuffer::Destroy()
{
  colorTexture.reset();
  depthTexture.reset();

  if (fbo != 0)
  {
    glDeleteFramebuffers(1, &fbo);
    fbo = 0;
  }

  width = 0;
  height = 0;
}
