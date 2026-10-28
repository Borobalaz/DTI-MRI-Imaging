#include "Passes/SkyboxPass.h"

#include <glad/glad.h>

#include "foundation/log/EngineLog.h"
#include "Render/Core/RenderCommands.h"
#include "Render/Geometry/Geometry.h"
#include "Render/Texture/TextureCube.h"
#include "RenderBackend.h"

bool SkyboxPass::Supports(const RendererDescriptor& renderer) const
{
  return renderer.capabilities.Contains(RenderCapabilities::Rasterization);
}

void SkyboxPass::Execute(const RenderFrame& frame, RenderExecutionContext&)
{
  for (const SkyboxRenderCommand& command : frame.data.Read<SkyboxRenderCommand>())
  {
    if (!command.geometry || !command.shader || !command.cubemap)
    {
      EngineLog::WarningOnce("skipped draw command: missing geometry, shader, or cubemap", "SkyboxPass");
      continue;
    }

    GLboolean previousDepthWriteMask = GL_TRUE;
    GLint previousDepthFunc = GL_LESS;
    const GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthWriteMask);
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    Shader& shader = *command.shader;
    RenderBackend::Use(shader);
    IUniformSink& uniforms = RenderBackend::Uniforms(shader);
    command.frameUniforms.Apply(uniforms);
    shader.Apply(uniforms);
    uniforms.SetTexture("skyboxTexture", command.cubemap.get(), 5);
    RenderBackend::Draw(*command.geometry, shader);
    if (cullFaceEnabled) glEnable(GL_CULL_FACE);
    glDepthMask(previousDepthWriteMask);
    glDepthFunc(previousDepthFunc);
  }
}