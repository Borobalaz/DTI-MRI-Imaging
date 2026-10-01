#pragma once
#include <memory>
#include <vector>
#include "Postprocessing/IPostProcessingEffect.h"
#include "RenderCore/RenderFrame.h"
#include "RenderCore/RendererDescriptor.h"
#include "Renderer/RenderPass.h"

class Renderer
{
public:
  Renderer() = default;
  virtual const RendererDescriptor& GetDescriptor() const = 0;
  virtual void Draw(const RenderFrame& frame) = 0;
  void AddPostProcessingEffect(std::shared_ptr<IPostProcessingEffect> effect) { postProcessingEffects.push_back(effect); }
  void ClearPostProcessingEffects( ) { postProcessingEffects.clear(); }
  void AddRenderPass(std::unique_ptr<IRenderPass> pass)
  {
    if (pass)
    {
      renderPasses.push_back(std::move(pass));
    }
  }

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

private:
  std::vector<std::shared_ptr<IPostProcessingEffect>> postProcessingEffects;
  std::vector<std::unique_ptr<IRenderPass>> renderPasses;
};