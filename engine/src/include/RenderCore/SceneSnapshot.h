#pragma once

#include <utility>
#include <vector>

#include "Renderer/RenderProxy.h"

class SceneSnapshot
{
public:
  SceneSnapshot() = default;
  explicit SceneSnapshot(std::vector<RenderProxy> renderProxies)
    : proxies(std::move(renderProxies))
  {
  }

  const std::vector<RenderProxy>& GetRenderProxies() const
  {
    return proxies;
  }

private:
  std::vector<RenderProxy> proxies;
};
