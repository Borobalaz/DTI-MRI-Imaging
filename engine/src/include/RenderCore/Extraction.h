#pragma once

#include <memory>
#include <string>
#include <vector>

#include "RenderCore/RenderDataStore.h"
#include "RenderCore/RendererDescriptor.h"
#include "RenderCore/SceneSnapshot.h"

class Diagnostics
{
public:
  void Add(const std::string& message)
  {
    messages.push_back(message);
  }

  const std::vector<std::string>& Messages() const
  {
    return messages;
  }

private:
  std::vector<std::string> messages;
};

class RenderExtractionContext
{
public:
  explicit RenderExtractionContext(const RendererDescriptor& rendererDescriptor)
    : rendererDescriptor(rendererDescriptor)
  {
  }

  const RendererDescriptor& Renderer() const
  {
    return rendererDescriptor;
  }

  Diagnostics& DiagnosticsLog()
  {
    return diagnostics;
  }

  const Diagnostics& DiagnosticsLog() const
  {
    return diagnostics;
  }

private:
  const RendererDescriptor& rendererDescriptor;
  Diagnostics diagnostics;
};

class IRenderExtractor
{
public:
  virtual ~IRenderExtractor() = default;

  virtual bool Supports(const RendererDescriptor& renderer) const = 0;
  virtual void Extract(const SceneSnapshot& scene,
                       RenderExtractionContext& context,
                       RenderDataStore& output) const = 0;
};
