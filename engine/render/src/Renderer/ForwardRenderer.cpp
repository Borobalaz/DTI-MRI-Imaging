#include "Render/Renderer/ForwardRenderer.h"

#include <glad/glad.h>

#include "Postprocessing/AntiAliasingPass.h"
#include "OpenGL/Framebuffer.h"
#include "Passes/InstancedMeshGeometryPass.h"
#include "Passes/MeshGeometryPass.h"
#include "Passes/SkyboxPass.h"
#include "Passes/VolumePass.h"
#include "Render/RenderTarget.h"
#include "RenderBackend.h"

ForwardRenderer::ForwardRenderer()
  : descriptor{
      typeId<ForwardRenderer>(),
      "Forward Renderer",
      CapabilitySet{RenderCapabilities::Rasterization, RenderCapabilities::ForwardShading}},
    sceneFramebuffer(std::make_unique<Framebuffer>(true)),
    postProcessScratchA(std::make_unique<Framebuffer>(false)),
    postProcessScratchB(std::make_unique<Framebuffer>(false))
{
  AddRenderPass(std::make_unique<MeshGeometryPass>());
  AddRenderPass(std::make_unique<InstancedMeshGeometryPass>());
  AddRenderPass(std::make_unique<VolumePass>());
  AddRenderPass(std::make_unique<SkyboxPass>());

  AddPostProcessingPass(std::make_unique<AntiAliasingPass>());
}

// Out of line so the unique_ptr<Framebuffer> members are destroyed where Framebuffer is complete.
ForwardRenderer::~ForwardRenderer() = default;

const RendererDescriptor& ForwardRenderer::GetDescriptor() const
{
  return descriptor;
}

void ForwardRenderer::Resize(int width, int height)
{
  viewportWidth = width;
  viewportHeight = height;
}

void ForwardRenderer::InitializeGLState()
{
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

void ForwardRenderer::Draw(const RenderFrame& frame)
{
  RenderExecutionContext executionContext(GetDescriptor());

  const bool usePostProcessing = postProcessingEnabled
      && HasPostProcessingPasses() && viewportWidth > 0 && viewportHeight > 0;
  GLint originalFbo = 0;

  if (usePostProcessing)
  {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFbo);
    sceneFramebuffer->Resize(viewportWidth, viewportHeight);
    sceneFramebuffer->Bind();
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
    ExecutePostProcessingPasses(sceneFramebuffer->GetColorTexture(),
                                 viewportWidth, viewportHeight,
                                 *postProcessScratchA, *postProcessScratchB,
                                 static_cast<unsigned int>(originalFbo));
  }
}

void ForwardRenderer::Draw(const RenderFrame& frame, const RenderTarget& target)
{
  Resize(target.GetWidth(), target.GetHeight());
  // Draw() treats the framebuffer bound on entry as its final output, so binding the
  // target here is enough for both the direct and the postprocessed path.
  RenderBackend::BindTarget(target);
  Draw(frame);
}

void ForwardRenderer::SetFillColor(const glm::vec3& color)
{
  fillColor = color;
}

void ForwardRenderer::SetPostProcessingEnabled(bool enabled)
{
  postProcessingEnabled = enabled;
}
