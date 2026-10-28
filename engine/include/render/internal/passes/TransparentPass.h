#pragma once

#include "render/renderer/RenderPass.h"

// Alpha-blended meshes (TransparentMeshRenderBatch), drawn last so they composite over everything.
class TransparentPass final : public IRenderPass
{
public:
  const char* Name() const override { return "TransparentPass"; }
  bool Supports(const RendererDescriptor& renderer) const override;
  void Execute(const RenderFrame& frame, RenderExecutionContext& context) override;
};
