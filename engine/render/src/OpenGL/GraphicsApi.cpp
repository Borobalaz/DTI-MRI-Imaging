#include "Render/GraphicsApi.h"

#include <glad/glad.h>

bool InitializeGraphicsApi(GraphicsProcLoader loadProc)
{
  return gladLoadGLLoader(loadProc) != 0;
}
