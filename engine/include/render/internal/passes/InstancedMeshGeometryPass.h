#pragma once

#include "render/renderer/RenderPass.h"

class InstancedMeshGeometryPass final : public IRenderPass
{
public:
  const char* Name() const override { return "InstancedMeshGeometryPass"; }
  bool Supports(const RendererDescriptor& renderer) const override;
  void Execute(const RenderFrame& frame, RenderExecutionContext& context) override;
};
