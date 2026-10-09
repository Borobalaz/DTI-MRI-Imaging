#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include "Postprocessing/IPostProcessingPass.h"
#include "RenderCore/Framebuffer.h"
#include "RenderCore/RenderFrame.h"
#include "RenderCore/RendererDescriptor.h"
#include "Renderer/RenderPass.h"

class Renderer
{
public:
  Renderer() = default;
  virtual const RendererDescriptor& GetDescriptor() const = 0;
  virtual void Draw(const RenderFrame& frame) = 0;

  // Default no-op; overridden by renderers that own viewport-sized GL
  // resources (e.g. ForwardRenderer's offscreen postprocessing framebuffers).
  virtual void Resize(int width, int height) {}

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
                                    unsigned int finalTargetFbo) const
  {
    const Texture* currentInput = &sceneColor;
    Framebuffer* scratch[2] = {&scratchA, &scratchB};
    int scratchIndex = 0;

    for (std::size_t i = 0; i < postProcessingPasses.size(); ++i)
    {
      const bool isLast = (i + 1 == postProcessingPasses.size());

      if (isLast)
      {
        glBindFramebuffer(GL_FRAMEBUFFER, finalTargetFbo);
        glViewport(0, 0, width, height);
      }
      else
      {
        scratch[scratchIndex]->Resize(width, height);
        scratch[scratchIndex]->Bind();
      }

      postProcessingPasses[i]->Apply(*currentInput, width, height);

      if (!isLast)
      {
        currentInput = &scratch[scratchIndex]->GetColorTexture();
        scratchIndex = 1 - scratchIndex;
      }
    }
  }

private:
  std::vector<std::unique_ptr<IRenderPass>> renderPasses;
  std::vector<std::unique_ptr<IPostProcessingPass>> postProcessingPasses;
};
