#pragma once

#include <memory>

#include "IDrawable.h"
#include "Renderer/RenderProxy.h"
#include "Render/Geometry/Geometry.h"
#include "Render/Material.h"
#include "Render/Uniform/UniformProvider.h"

class Mesh: public IDrawable
{
public:
  Mesh(std::shared_ptr<Geometry> geometry,
       std::shared_ptr<Material> material);

  void SetGeometry(std::shared_ptr<Geometry> geometry);
  void SetMaterial(std::shared_ptr<Material> material);
  std::shared_ptr<Geometry> GetGeometry() const { return geometry; }
  std::shared_ptr<Material> GetMaterial() const { return material; }

  void BuildRenderProxy(RenderProxy& context) const override { (void)context; }

private:
  std::shared_ptr<Geometry> geometry;
  std::shared_ptr<Material> material;
};