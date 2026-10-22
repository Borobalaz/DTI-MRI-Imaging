#pragma once

#include <memory>

#include "Geometry/CubeGeometry.h"
#include "Render/Shader.h"
#include "Render/Texture/TextureCube.h"
#include "../IDrawable.h"

class Skybox : public IDrawable
{
public:
  explicit Skybox(std::shared_ptr<TextureCube> cubemap);

  void BuildRenderProxy(RenderProxy& renderProxy) const override;
  bool IsValid() const;

private:
  std::shared_ptr<TextureCube> cubemap;
  std::shared_ptr<CubeGeometry> geometry;
  std::shared_ptr<Shader> shader;
};