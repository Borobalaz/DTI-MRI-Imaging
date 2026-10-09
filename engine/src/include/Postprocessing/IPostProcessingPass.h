#pragma once

#include "Texture/Texture.h"

// A single fullscreen postprocessing step. Passes are intentionally dumb:
// given an input color texture and the current viewport size, draw into
// whatever framebuffer the orchestrator (Renderer::ExecutePostProcessingPasses)
// has already bound and sized the viewport for. A pass must never bind a
// framebuffer or set the viewport itself -- that stays the orchestrator's
// job, so a pass stays a reusable chain link regardless of whether its
// output lands on an intermediate scratch buffer or the final target.
class IPostProcessingPass
{
public:
  virtual ~IPostProcessingPass() = default;

  virtual const char* Name() const = 0;
  virtual void Apply(const Texture& input, int viewportWidth, int viewportHeight) = 0;
};
