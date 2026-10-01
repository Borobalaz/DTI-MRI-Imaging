#pragma once

#include "Renderer/RenderPass.h"

class MeshGeometryPass final : public IRenderPass
{
public:
  const char* Name() const override { return "MeshGeometryPass"; }
  bool Supports(const RendererDescriptor& renderer) const override;
  void Execute(const RenderFrame& frame, RenderExecutionContext& context) override;
};