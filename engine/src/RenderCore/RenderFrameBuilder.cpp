#include "RenderCore/RenderFrameBuilder.h"

RenderFrame RenderFrameBuilder::Build(const SceneSnapshot& snapshot,
                                      const RendererDescriptor& renderer,
                                      const ExtractionRegistry& extractionRegistry) const
{
  RenderFrame frame;
  RenderExtractionContext context(renderer);
  for (const RenderProxy& proxy : snapshot.GetRenderProxies())
  {
    if (proxy.visible)
    {
      frame.data.AppendFrom(proxy.data);
    }
  }
  extractionRegistry.ExtractFor(renderer, snapshot, context, frame.data);
  return frame;
}
