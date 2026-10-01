#pragma once

#include "Renderer/RenderPass.h"

class VolumePass final : public IRenderPass
{
public:
  const char* Name() const override { return "VolumePass"; }
  bool Supports(const RendererDescriptor& renderer) const override;
  void Execute(const RenderFrame& frame, RenderExecutionContext& context) override;
};