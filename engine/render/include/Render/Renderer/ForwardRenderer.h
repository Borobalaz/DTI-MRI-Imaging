#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Render/Renderer/Renderer.h"

class Framebuffer;

class ForwardRenderer : public Renderer
{
public:
  ForwardRenderer();
  ~ForwardRenderer();

  const RendererDescriptor& GetDescriptor() const override;
  void Draw(const RenderFrame& frame) override;
  void Draw(const RenderFrame& frame, const RenderTarget& target) override;
  void Resize(int width, int height) override;
  void InitializeGLState() override;

  void SetFillColor(const glm::vec3& color);

  // Gates the registered postprocessing chain; when disabled the scene
  // passes render straight into the caller's framebuffer.
  void SetPostProcessingEnabled(bool enabled);

private:
  RendererDescriptor descriptor;
  glm::vec3 fillColor{0.0f};
  bool postProcessingEnabled = true;

  int viewportWidth = 0;
  int viewportHeight = 0;

  std::unique_ptr<Framebuffer> sceneFramebuffer;
  std::unique_ptr<Framebuffer> postProcessScratchA;
  std::unique_ptr<Framebuffer> postProcessScratchB;
};
