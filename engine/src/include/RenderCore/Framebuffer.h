#pragma once

#include <memory>

#include <glad/glad.h>

#include "Texture/Texture2D.h"

// Owns an offscreen GL framebuffer object with a color attachment (always)
// and an optional depth attachment. Used to render the scene offscreen
// before postprocessing, and as ping-pong scratch targets between chained
// postprocessing passes.
class Framebuffer
{
public:
  explicit Framebuffer(bool withDepth);
  ~Framebuffer();

  Framebuffer(const Framebuffer&) = delete;
  Framebuffer& operator=(const Framebuffer&) = delete;

  // (Re)allocates GL resources if the size differs from the current size
  // (or nothing has been allocated yet). Safe to call every frame. No-op on
  // non-positive size.
  void Resize(int width, int height);

  // Binds this FBO as the current draw target and sets the viewport to
  // match. Precondition: Resize() has succeeded at least once.
  void Bind() const;

  // Precondition: Resize() has succeeded at least once.
  const Texture2D& GetColorTexture() const { return *colorTexture; }

  int GetWidth() const { return width; }
  int GetHeight() const { return height; }
  bool IsValid() const { return fbo != 0; }

private:
  void Allocate(int w, int h);
  void Destroy();

  unsigned int fbo = 0;
  std::unique_ptr<Texture2D> colorTexture;
  std::unique_ptr<Texture2D> depthTexture;
  bool withDepth;
  int width = 0;
  int height = 0;
};
