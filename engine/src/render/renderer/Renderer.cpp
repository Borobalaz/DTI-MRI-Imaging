#include "render/renderer/Renderer.h"

#include <glad/glad.h>

#include "render/internal/opengl/Framebuffer.h"

void Renderer::ExecutePostProcessingPasses(const Texture& sceneColor,
                                           int width, int height,
                                           Framebuffer& scratchA, Framebuffer& scratchB,
                                           unsigned int finalTargetFbo) const
{
  const Texture* currentInput = &sceneColor;
  Framebuffer* scratch[2] = {&scratchA, &scratchB};
  int scratchIndex = 0;

  for (std::size_t i = 0; i < postProcessingPasses.size(); ++i)
  {
    const bool isLast = (i + 1 == postProcessingPasses.size());

    if (isLast)
    {
      glBindFramebuffer(GL_FRAMEBUFFER, finalTargetFbo);
      glViewport(0, 0, width, height);
    }
    else
    {
      scratch[scratchIndex]->Resize(width, height);
      scratch[scratchIndex]->Bind();
    }

    postProcessingPasses[i]->Apply(*currentInput, width, height);

    if (!isLast)
    {
      currentInput = &scratch[scratchIndex]->GetColorTexture();
      scratchIndex = 1 - scratchIndex;
    }
  }
}
