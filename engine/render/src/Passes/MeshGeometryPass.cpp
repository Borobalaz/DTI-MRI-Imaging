#include "Passes/MeshGeometryPass.h"

#include "Log/EngineLog.h"
#include "Render/Core/RenderCommands.h"
#include "Render/Geometry/Geometry.h"
#include "Render/Material.h"
#include "RenderBackend.h"

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
        EngineLog::WarningOnce("skipped draw command: missing geometry or material", "MeshGeometryPass");
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
}