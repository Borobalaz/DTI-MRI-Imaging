#pragma once

#include "RenderCore/ExtractionRegistry.h"
#include "Render/Core/RenderFrame.h"

class RenderFrameBuilder
{
public:
  RenderFrame Build(const SceneSnapshot& snapshot,
                    const RendererDescriptor& renderer,
                    const ExtractionRegistry& extractionRegistry) const;
};
