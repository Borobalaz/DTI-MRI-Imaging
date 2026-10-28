#include "OpenGL/Framebuffer.h"

#include <iostream>

#include <glad/glad.h>

#include "foundation/log/EngineLog.h"
#include "OpenGL/GpuDeletionQueue.h"
#include "RenderBackend.h"

Framebuffer::Framebuffer(bool withDepth)
  : withDepth(withDepth)
{
}

// May run on any thread, so the FBO is queued for deletion on the GL thread (the
// attachment textures queue themselves from their own destructors).
Framebuffer::~Framebuffer()
{
  EnqueueGpuDeletion(GpuResourceKind::Framebuffer, fbo);
}

void Framebuffer::Allocate(int w, int h)
{
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  colorTexture = std::make_unique<Texture2D>(w, h, TextureFormat::RGBA8, nullptr, true);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, RenderBackend::Handle(*colorTexture), 0);

  if (withDepth)
  {
    depthTexture = std::make_unique<Texture2D>(w, h, TextureFormat::Depth24, nullptr, false);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, RenderBackend::Handle(*depthTexture), 0);
  }

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    std::cout << "Framebuffer incomplete (" << w << "x" << h << ")" << std::endl;
    EngineLog::Error("Framebuffer incomplete (" + std::to_string(w) + "x" + std::to_string(h) + ")",
                      "Framebuffer");
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
