#pragma once

#include <memory>

#include "Postprocessing/IPostProcessingPass.h"
#include "Postprocessing/PostProcessingShader.h"

// Basic FXAA-style single-pass antialiasing, applied as the final
// postprocessing step over the resolved scene color.
class AntiAliasingPass final : public IPostProcessingPass
{
public:
  const char* Name() const override { return "AntiAliasingPass"; }
  void Apply(const Texture& input, int viewportWidth, int viewportHeight) override;

private:
  // Built lazily on first Apply(): this pass is owned by ForwardRenderer,
  // which is constructed before the GL context exists, so building the
  // Shader eagerly here would compile against a not-yet-current context.
  std::unique_ptr<PostProcessingShader> shader;
};
