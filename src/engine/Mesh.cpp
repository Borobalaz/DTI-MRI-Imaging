#include "Mesh.h"

Mesh::Mesh(std::shared_ptr<Geometry> geometry,
           std::shared_ptr<Material> material)
  : geometry(std::move(geometry)),
    material(std::move(material))
{
}

/**
 * @brief Set the argument pointer's destination as the mesh's geometry field pointer destination. 
 *        The argument pointer can be recycled.
 * 
 * @param geometry 
 */
void Mesh::SetGeometry(std::shared_ptr<Geometry> geometry)
{
  this->geometry = std::move(geometry);
}

/**
 * @brief Set the argument pointer's destination as the mesh's material field pointer destination. 
 *        The argument pointer can be recycled.
 * 
 * @param material 
 */
void Mesh::SetMaterial(std::shared_ptr<Material> material)
{
  this->material = std::move(material);
}

/**
 * @brief Render the mesh.
 * 
 * @param uniformProvider 
 */
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