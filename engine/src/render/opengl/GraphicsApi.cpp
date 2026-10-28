#include "render/GraphicsApi.h"

#include <glad/glad.h>

#include "render/internal/opengl/GpuDeletionQueue.h"

bool InitializeGraphicsApi(GraphicsProcLoader loadProc)
{
  if (gladLoadGLLoader(loadProc) == 0)
  {
    return false;
  }

  SetGpuDeletionAccepting(true);
  return true;
}

void CollectGraphicsGarbage()
{
  FlushGpuDeletions();
}

void ShutdownGraphicsApi()
{
  FlushGpuDeletions();
  SetGpuDeletionAccepting(false);
}
