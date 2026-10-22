#pragma once

#include "Render/Core/RenderCommands.h"
#include "Render/Core/RenderDataStore.h"
#include "Render/Uniform/CompositeUniformProvider.h"

struct RenderProxy
{
  RenderDataStore data;
  CompositeUniformProvider frameUniforms;
  bool visible = true;
};
