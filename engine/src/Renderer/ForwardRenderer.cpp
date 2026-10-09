#include "Renderer/ForwardRenderer.h"

#include <glad/glad.h>

#include "Postprocessing/Passes/AntiAliasingPass.h"
#include "Renderer/Passes/MeshGeometryPass.h"
#include "Renderer/Passes/SkyboxPass.h"
#include "Renderer/Passes/VolumePass.h"

ForwardRenderer::ForwardRenderer()
  : descriptor{
      typeId<ForwardRenderer>(),
      "Forward Renderer",
      CapabilitySet{RenderCapabilities::Rasterization, RenderCapabilities::ForwardShading}}
{
  AddRenderPass(std::make_unique<MeshGeometryPass>());
  AddRenderPass(std::make_unique<VolumePass>());
  AddRenderPass(std::make_unique<SkyboxPass>());

  AddPostProcessingPass(std::make_unique<AntiAliasingPass>());
}

const RendererDescriptor& ForwardRenderer::GetDescriptor() const
{
  return descriptor;
}

void ForwardRenderer::Resize(int width, int height)
{
  viewportWidth = width;
  viewportHeight = height;
}

void ForwardRenderer::Draw(const RenderFrame& frame)
{
  RenderExecutionContext executionContext(GetDescriptor());

  const bool usePostProcessing = HasPostProcessingPasses() && viewportWidth > 0 && viewportHeight > 0;
  GLint originalFbo = 0;

  if (usePostProcessing)
  {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFbo);
    sceneFramebuffer.Resize(viewportWidth, viewportHeight);
    sceneFramebuffer.Bind();
  }

  // Qt/driver may change state between frames; reset what the clear and passes rely on.
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_BLEND);
  glClearColor(fillColor.r, fillColor.g, fillColor.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  ExecuteAdditionalPasses(frame, executionContext);

  if (usePostProcessing)
  {
    glDisable(GL_DEPTH_TEST);
    ExecutePostProcessingPasses(sceneFramebuffer.GetColorTexture(),
                                 viewportWidth, viewportHeight,
                                 postProcessScratchA, postProcessScratchB,
                                 static_cast<unsigned int>(originalFbo));
  }
}

void ForwardRenderer::SetFillColor(const glm::vec3& color)
{
  fillColor = color;
}
