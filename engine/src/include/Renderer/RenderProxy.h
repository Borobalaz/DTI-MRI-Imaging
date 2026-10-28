#pragma once

#include "render/core/RenderCommands.h"
#include "render/core/RenderDataStore.h"
#include "render/uniform/CompositeUniformProvider.h"

struct RenderProxy
{
  RenderDataStore data;
  CompositeUniformProvider frameUniforms;
  bool visible = true;
};
