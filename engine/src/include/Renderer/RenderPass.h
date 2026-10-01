#pragma once

#include <memory>
#include <vector>

#include "RenderCore/RenderFrame.h"
#include "RenderCore/RendererDescriptor.h"

class RenderExecutionContext
{
public:
  explicit RenderExecutionContext(const RendererDescriptor& rendererDescriptor)
    : renderer(rendererDescriptor)
  {
  }

  const RendererDescriptor& Renderer() const
  {
    return renderer;
  }

private:
  const RendererDescriptor& renderer;
};

class IRenderPass
{
public:
  virtual ~IRenderPass() = default;

  virtual const char* Name() const = 0;
  virtual bool Supports(const RendererDescriptor& renderer) const = 0;
  virtual void Execute(const RenderFrame& frame, RenderExecutionContext& context) = 0;
};
