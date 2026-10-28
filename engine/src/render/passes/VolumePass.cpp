#include "render/internal/passes/VolumePass.h"

#include <glad/glad.h>

#include "foundation/log/EngineLog.h"
#include "render/core/RenderCommands.h"
#include "render/geometry/Geometry.h"
#include "render/VolumeTextureSet.h"
#include "render/internal/RenderBackend.h"

bool VolumePass::Supports(const RendererDescriptor& renderer) const
{
  return renderer.capabilities.Contains(RenderCapabilities::Rasterization);
}

void VolumePass::Execute(const RenderFrame& frame, RenderExecutionContext&)
{
  for (const VolumeRenderCommand& command : frame.data.Read<VolumeRenderCommand>())
  {
    if (!command.geometry || !command.shader || !command.textures)
    {
      EngineLog::WarningOnce("skipped draw command: missing geometry, shader, or textures", "VolumePass");
      continue;
    }

    const GLboolean previousBlendEnabled = glIsEnabled(GL_BLEND);
    GLboolean previousDepthWriteMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthWriteMask);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    Shader& shader = *command.shader;
    RenderBackend::Use(shader);
    IUniformSink& uniforms = RenderBackend::Uniforms(shader);
    command.frameUniforms.Apply(uniforms);
    uniforms.SetVec3("volume.dimensions", glm::vec3(command.dimensions));
    uniforms.SetMat4("volumeObject.modelMatrix", command.modelMatrix);
    uniforms.SetMat4("volumeObject.inverseModelMatrix", command.inverseModelMatrix);
    command.textures->Bind(uniforms, "volumeTextures");
    shader.Apply(uniforms);
    RenderBackend::Draw(*command.geometry, shader);
    if (!previousBlendEnabled) glDisable(GL_BLEND);
    glDepthMask(previousDepthWriteMask);
  }
}