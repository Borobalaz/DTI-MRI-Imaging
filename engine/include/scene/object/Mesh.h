#pragma once

#include <memory>

#include "scene/object/IDrawable.h"
#include "scene/extraction/RenderProxy.h"
#include "render/geometry/Geometry.h"
#include "render/Material.h"
#include "render/uniform/UniformProvider.h"

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