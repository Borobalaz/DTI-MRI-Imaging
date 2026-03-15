#include "Mesh.h"

Mesh::Mesh(std::shared_ptr<Geometry> geometry,
           std::shared_ptr<Material> material)
  : geometry(std::move(geometry)),
    material(std::move(material))
{
}

void Mesh::SetGeometry(std::shared_ptr<Geometry> geometry)
{
  this->geometry = std::move(geometry);
}

void Mesh::SetMaterial(std::shared_ptr<Material> material)
{
  this->material = std::move(material);
}

void Mesh::Draw(const UniformProvider& uniformProvider) const
{
  if (!geometry || !material)
  {
    return;
  }

  material->Bind();

  Shader& shader = material->GetShader();
  uniformProvider.Apply(shader);
  geometry->Draw(shader);
}