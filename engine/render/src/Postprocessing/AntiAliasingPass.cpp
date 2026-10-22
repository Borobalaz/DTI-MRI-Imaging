#include "Postprocessing/AntiAliasingPass.h"

#include "RenderBackend.h"

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
  RenderBackend::Use(fxaaShader);
  IUniformSink& uniforms = RenderBackend::Uniforms(fxaaShader);
  uniforms.SetTexture("sceneColor", &input, kSceneColorUnit);

  const glm::vec2 invScreenSize(
    viewportWidth > 0 ? 1.0f / static_cast<float>(viewportWidth) : 0.0f,
    viewportHeight > 0 ? 1.0f / static_cast<float>(viewportHeight) : 0.0f);
  uniforms.SetVec2("invScreenSize", invScreenSize);

  shader->DrawFullscreenTriangle();
}
