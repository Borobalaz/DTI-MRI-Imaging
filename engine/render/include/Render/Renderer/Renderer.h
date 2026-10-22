#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include "Render/Postprocessing/IPostProcessingPass.h"
#include "Render/Core/RenderFrame.h"
#include "Render/Core/RendererDescriptor.h"
#include "Render/Renderer/RenderPass.h"

class Framebuffer;
class RenderTarget;

class Renderer
{
public:
  Renderer() = default;
  virtual const RendererDescriptor& GetDescriptor() const = 0;
  // Draws into whatever framebuffer is bound on entry (the caller sets the viewport).
  virtual void Draw(const RenderFrame& frame) = 0;

  // Draws into `target` at the target's size; postprocessing composites into it too.
  // Precondition: target.Resize() has succeeded at least once.
  virtual void Draw(const RenderFrame& frame, const RenderTarget& target) = 0;

  // Default no-op; overridden by renderers that own viewport-sized GL
  // resources (e.g. ForwardRenderer's offscreen postprocessing framebuffers).
  virtual void Resize(int width, int height) {}

  // One-time global GL state setup. Invoked exactly once by
  // Engine::InitializeOpenGL, after GLAD function pointers are confirmed
  // loaded and before any frame is drawn.
  virtual void InitializeGLState() {}

  void AddRenderPass(std::unique_ptr<IRenderPass> pass)
  {
    if (pass)
    {
      renderPasses.push_back(std::move(pass));
    }
  }

  void AddPostProcessingPass(std::unique_ptr<IPostProcessingPass> pass)
  {
    if (pass)
    {
      postProcessingPasses.push_back(std::move(pass));
    }
  }

  void ClearPostProcessingPasses() { postProcessingPasses.clear(); }
  bool HasPostProcessingPasses() const { return !postProcessingPasses.empty(); }

protected:
  void ExecuteAdditionalPasses(const RenderFrame& frame,
                               RenderExecutionContext& context) const
  {
    for (const std::unique_ptr<IRenderPass>& pass : renderPasses)
    {
      if (pass && pass->Supports(GetDescriptor()))
      {
        pass->Execute(frame, context);
      }
    }
  }

  // Walks the registered postprocessing chain. `sceneColor` feeds the first
  // pass. `scratchA`/`scratchB` are caller-owned color-only framebuffers used
  // to ping-pong between intermediate passes -- never resized/allocated when
  // the chain has 0 or 1 passes. The last pass's output is bound to
  // `finalTargetFbo` (the FBO bound at Draw() entry) with an explicit
  // viewport, restoring the state the caller had set up before Draw() ran.
  void ExecutePostProcessingPasses(const Texture& sceneColor,
                                    int width, int height,
                                    Framebuffer& scratchA, Framebuffer& scratchB,
                                    unsigned int finalTargetFbo) const;

private:
  std::vector<std::unique_ptr<IRenderPass>> renderPasses;
  std::vector<std::unique_ptr<IPostProcessingPass>> postProcessingPasses;
};
