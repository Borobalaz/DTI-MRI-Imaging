#include "render/internal/postprocessing/PostProcessingShader.h"

#include "render/internal/RenderBackend.h"

namespace
{
  constexpr const char* kPostProcessVertexPath = "shaders/postprocessing/postprocess_vertex.glsl";
}

PostProcessingShader::PostProcessingShader(const std::string& id, const std::string& fragmentPath)
  : shader(id, kPostProcessVertexPath, fragmentPath)
{
}

void PostProcessingShader::DrawFullscreenTriangle()
{
  RenderBackend::Draw(geometry, shader);
}
