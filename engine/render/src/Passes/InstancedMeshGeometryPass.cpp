#include "Passes/InstancedMeshGeometryPass.h"

#include "Log/EngineLog.h"
#include "Render/Core/RenderCommands.h"
#include "Render/Geometry/Geometry.h"
#include "Render/Geometry/InstanceBuffer.h"
#include "Render/Material.h"
#include "RenderBackend.h"

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
      if (!command.geometry || !command.material || !command.instances || !command.instances->IsValid())
      {
        EngineLog::WarningOnce("skipped instanced draw command: missing geometry/material or empty instance buffer", "InstancedMeshGeometryPass");
        continue;
      }

      Shader& shader = command.material->GetShader();
      RenderBackend::Use(shader);
      IUniformSink& uniforms = RenderBackend::Uniforms(shader);
      command.frameUniforms.Apply(uniforms);
      uniforms.SetMat4("gameObject.modelMatrix", command.modelMatrix);
      command.material->Apply(uniforms);
      shader.Apply(uniforms);
      command.instances->Apply(uniforms);
      RenderBackend::DrawInstanced(*command.geometry, shader, *command.instances);
    }
  }
}
