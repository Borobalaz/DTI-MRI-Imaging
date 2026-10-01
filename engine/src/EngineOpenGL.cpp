#include "EngineOpenGL.h"

bool InitializeEngineOpenGL(GLADloadproc loadProc)
{
  return gladLoadGLLoader(loadProc) != 0;
}
