#pragma once

#include "scene/extraction/ExtractionRegistry.h"
#include "render/core/RenderFrame.h"

class RenderFrameBuilder
{
public:
  RenderFrame Build(const SceneSnapshot& snapshot,
                    const RendererDescriptor& renderer,
                    const ExtractionRegistry& extractionRegistry) const;
};
