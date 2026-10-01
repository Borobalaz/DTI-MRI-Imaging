#include "Renderer/Passes/VolumePass.h"

#include <glad/glad.h>

#include "Geometry/VolumeGeometry.h"
#include "Renderer/RenderProxy.h"
#include "Volume/VolumeTextureSet.h"

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
      continue;
    }

    const GLboolean previousBlendEnabled = glIsEnabled(GL_BLEND);
    GLboolean previousDepthWriteMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthWriteMask);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    command.shader->Use();
    command.frameUniforms.Apply(*command.shader);
    command.shader->SetVec3("volume.dimensions", glm::vec3(command.dimensions));
    command.shader->SetMat4("volumeObject.modelMatrix", command.modelMatrix);
    command.shader->SetMat4("volumeObject.inverseModelMatrix", command.inverseModelMatrix);
    command.textures->Bind(*command.shader, "volumeTextures");
    command.shader->Apply(*command.shader);
    command.geometry->Draw(*command.shader);
    if (!previousBlendEnabled) glDisable(GL_BLEND);
    glDepthMask(previousDepthWriteMask);
  }
}