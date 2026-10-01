#include "Renderer/Passes/SkyboxPass.h"

#include <glad/glad.h>

#include "Geometry/CubeGeometry.h"
#include "Renderer/RenderProxy.h"
#include "Texture/TextureCube.h"

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
    command.shader->Use();
    command.frameUniforms.Apply(*command.shader);
    command.shader->Apply(*command.shader);
    command.shader->SetTexture("skyboxTexture", 5);
    command.cubemap->Bind(5);
    command.geometry->Draw(*command.shader);
    if (cullFaceEnabled) glEnable(GL_CULL_FACE);
    glDepthMask(previousDepthWriteMask);
    glDepthFunc(previousDepthFunc);
  }
}