#include "Postprocessing/Passes/AntiAliasingPass.h"

namespace
{
  constexpr unsigned int kSceneColorUnit = 0;
}

void AntiAliasingPass::Apply(const Texture& input, int viewportWidth, int viewportHeight)
{
  if (!shader)
  {
    shader = std::make_unique<PostProcessingShader>("fxaa_shader", "shaders/postprocessing/fxaa_fragment.glsl");
  }

  Shader& fxaaShader = shader->GetShader();
  fxaaShader.Use();
  fxaaShader.SetTexture("sceneColor", kSceneColorUnit);
  input.Bind(kSceneColorUnit);

  const glm::vec2 invScreenSize(
    viewportWidth > 0 ? 1.0f / static_cast<float>(viewportWidth) : 0.0f,
    viewportHeight > 0 ? 1.0f / static_cast<float>(viewportHeight) : 0.0f);
  fxaaShader.SetVec2("invScreenSize", invScreenSize);

  shader->DrawFullscreenTriangle();
}
