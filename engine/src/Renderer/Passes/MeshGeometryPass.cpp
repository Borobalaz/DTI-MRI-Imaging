#include "Renderer/Passes/MeshGeometryPass.h"

#include "Geometry/Geometry.h"
#include "Material.h"
#include "Renderer/RenderProxy.h"

bool MeshGeometryPass::Supports(const RendererDescriptor& renderer) const
{
  return renderer.capabilities.Contains(RenderCapabilities::Rasterization);
}

void MeshGeometryPass::Execute(const RenderFrame& frame, RenderExecutionContext&) 
{
  for (const MeshRenderBatch& batch : frame.data.Read<MeshRenderBatch>())
  {
    for (const MeshRenderCommand& command : batch.draws)
    {
      if (!command.geometry || !command.material)
      {
        continue;
      }

      Shader& shader = command.material->GetShader();
      shader.Use();
      command.frameUniforms.Apply(shader);
      shader.SetMat4("gameObject.modelMatrix", command.modelMatrix);
      command.material->Apply(shader);
      shader.Apply(shader);
      command.geometry->Draw(shader);
    }
  }
}