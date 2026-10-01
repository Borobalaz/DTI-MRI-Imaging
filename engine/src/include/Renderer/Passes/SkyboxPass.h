#pragma once

#include "Renderer/RenderPass.h"

class SkyboxPass final : public IRenderPass
{
public:
  const char* Name() const override { return "SkyboxPass"; }
  bool Supports(const RendererDescriptor& renderer) const override;
  void Execute(const RenderFrame& frame, RenderExecutionContext& context) override;
};