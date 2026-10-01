#pragma once

#include <glm/glm.hpp>

#include "Renderer.h"

class ForwardRenderer : public Renderer
{
public:
  ForwardRenderer();

  const RendererDescriptor& GetDescriptor() const override;
  void Draw(const RenderFrame& frame) override;

  void SetFillColor(const glm::vec3& color);

private:
  RendererDescriptor descriptor;
  glm::vec3 fillColor{0.0f};
};
