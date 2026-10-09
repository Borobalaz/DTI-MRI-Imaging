#pragma once

#include <glm/glm.hpp>

#include "RenderCore/Framebuffer.h"
#include "Renderer.h"

class ForwardRenderer : public Renderer
{
public:
  ForwardRenderer();

  const RendererDescriptor& GetDescriptor() const override;
  void Draw(const RenderFrame& frame) override;
  void Resize(int width, int height) override;

  void SetFillColor(const glm::vec3& color);

private:
  RendererDescriptor descriptor;
  glm::vec3 fillColor{0.0f};

  int viewportWidth = 0;
  int viewportHeight = 0;

  Framebuffer sceneFramebuffer{true};
  Framebuffer postProcessScratchA{false};
  Framebuffer postProcessScratchB{false};
};
