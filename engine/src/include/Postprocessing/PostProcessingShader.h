#pragma once

#include <string>

#include "Geometry/FullscreenTriangleGeometry.h"
#include "Shader.h"

// Bundles a Shader (built against the shared postprocessing vertex shader)
// with the fullscreen-triangle geometry used to invoke it. Concrete passes
// own one of these and only need to supply a fragment shader.
class PostProcessingShader
{
public:
  PostProcessingShader(const std::string& id, const std::string& fragmentPath);

  Shader& GetShader() { return shader; }

  void DrawFullscreenTriangle();

private:
  Shader shader;
  FullscreenTriangleGeometry geometry;
};
