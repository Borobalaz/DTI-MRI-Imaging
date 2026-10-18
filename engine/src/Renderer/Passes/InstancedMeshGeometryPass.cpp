#include "Renderer/Passes/InstancedMeshGeometryPass.h"

#include "Geometry/Geometry.h"
#include "Log/EngineLog.h"
#include "Material.h"
#include "Renderer/RenderProxy.h"

bool InstancedMeshGeometryPass::Supports(const RendererDescriptor& renderer) const
{
  return renderer.capabilities.Contains(RenderCapabilities::Rasterization);
}

void InstancedMeshGeometryPass::Execute(const RenderFrame& frame, RenderExecutionContext&)
{
  for (const MeshInstancedRenderBatch& batch : frame.data.Read<MeshInstancedRenderBatch>())
  {
    for (const MeshInstancedRenderCommand& command : batch.draws)
    {
      if (!command.geometry || !command.material || command.instanceVBO == 0 || command.instanceCount == 0)
      {
        EngineLog::WarningOnce("skipped instanced draw command: missing geometry/material or empty instance buffer", "InstancedMeshGeometryPass");
        continue;
      }

      Shader& shader = command.material->GetShader();
      shader.Use();
      command.frameUniforms.Apply(shader);
      shader.SetMat4("gameObject.modelMatrix", command.modelMatrix);
      command.material->Apply(shader);
      shader.Apply(shader);
      command.geometry->DrawInstanced(shader, command.instanceVBO, command.instanceCount);
    }
  }
}
