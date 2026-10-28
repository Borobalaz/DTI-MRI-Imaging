#include "render/internal/passes/MeshGeometryPass.h"

#include <glad/glad.h>

#include "foundation/log/EngineLog.h"
#include "render/core/RenderCommands.h"
#include "render/geometry/Geometry.h"
#include "render/Material.h"
#include "render/internal/RenderBackend.h"

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

      const bool cullBackFaces = command.material->GetCullBackFaces();
      GLboolean previousCullFaceEnabled = GL_FALSE;
      GLint previousCullFaceMode = GL_BACK;
      GLint previousFrontFace = GL_CCW;
      if (cullBackFaces)
      {
        previousCullFaceEnabled = glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_CULL_FACE_MODE, &previousCullFaceMode);
        glGetIntegerv(GL_FRONT_FACE, &previousFrontFace);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
      }

      RenderBackend::Draw(*command.geometry, shader);

      if (cullBackFaces)
      {
        if (!previousCullFaceEnabled)
        {
          glDisable(GL_CULL_FACE);
        }
        glCullFace(static_cast<GLenum>(previousCullFaceMode));
        glFrontFace(static_cast<GLenum>(previousFrontFace));
      }
    }
  }
}