#include "render/internal/passes/TransparentPass.h"

#include <glad/glad.h>

#include "foundation/log/EngineLog.h"
#include "render/core/RenderCommands.h"
#include "render/geometry/Geometry.h"
#include "render/Material.h"
#include "render/internal/RenderBackend.h"

bool TransparentPass::Supports(const RendererDescriptor& renderer) const
{
  return renderer.capabilities.Contains(RenderCapabilities::Rasterization);
}

void TransparentPass::Execute(const RenderFrame& frame, RenderExecutionContext&)
{
  const std::vector<TransparentMeshRenderBatch>& batches = frame.data.Read<TransparentMeshRenderBatch>();
  if (batches.empty())
  {
    return; // no state touched when there is nothing to draw
  }

  const GLboolean previousBlendEnabled = glIsEnabled(GL_BLEND);
  GLint previousBlendSrcRgb = GL_ONE;
  GLint previousBlendDstRgb = GL_ZERO;
  GLint previousBlendSrcAlpha = GL_ONE;
  GLint previousBlendDstAlpha = GL_ZERO;
  glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
  glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);
  const GLboolean previousDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
  GLboolean previousDepthWriteMask = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthWriteMask);
  const GLboolean previousCullFaceEnabled = glIsEnabled(GL_CULL_FACE);

  // Destination alpha is kept (ZERO, ONE) so the fill never makes the published frame see-through.
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);

  for (const TransparentMeshRenderBatch& batch : batches)
  {
    for (const TransparentMeshRenderCommand& command : batch.draws)
    {
      if (!command.geometry || !command.material)
      {
        EngineLog::WarningOnce("skipped transparent draw command: missing geometry or material", "TransparentPass");
        continue;
      }

      Shader& shader = command.material->GetShader();
      RenderBackend::Use(shader);
      IUniformSink& uniforms = RenderBackend::Uniforms(shader);
      command.frameUniforms.Apply(uniforms);
      uniforms.SetMat4("gameObject.modelMatrix", command.modelMatrix);
      command.material->Apply(uniforms);
      shader.Apply(uniforms);
      RenderBackend::Draw(*command.geometry, shader);
    }
  }

  if (!previousBlendEnabled)
  {
    glDisable(GL_BLEND);
  }
  glBlendFuncSeparate(static_cast<GLenum>(previousBlendSrcRgb), static_cast<GLenum>(previousBlendDstRgb),
                      static_cast<GLenum>(previousBlendSrcAlpha), static_cast<GLenum>(previousBlendDstAlpha));
  if (!previousDepthTestEnabled)
  {
    glDisable(GL_DEPTH_TEST);
  }
  glDepthMask(previousDepthWriteMask);
  if (previousCullFaceEnabled)
  {
    glEnable(GL_CULL_FACE);
  }
}
