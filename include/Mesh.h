#pragma once

#include <memory>

#include "Geometry.h"
#include "Material.h"
#include "UniformProvider.h"

class Mesh
{
public:
  Mesh(std::shared_ptr<Geometry> geometry,
       std::shared_ptr<Material> material);

  void SetGeometry(std::shared_ptr<Geometry> geometry);
  void SetMaterial(std::shared_ptr<Material> material);

  void Draw(const UniformProvider& uniformProvider) const;

private:
  std::shared_ptr<Geometry> geometry;
  std::shared_ptr<Material> material;
};