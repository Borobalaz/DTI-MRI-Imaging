#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include "Mesh.h"
#include "IDrawable.h"
#include "UniformProvider.h"

class GameObject : public UniformProvider, public IDrawable, public IInspectable
{
public:
  GameObject();
  ~GameObject();

  void AddMesh(std::shared_ptr<Mesh> mesh);

  void Update(float deltaTime);
  void Draw(const UniformProvider& frameUniforms) const override;
  void Apply(Shader& shader) const override;
  void CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix) override;
  void CollectInspectableNodes(std::vector<InspectableNode>& out, const std::string& nodePrefix) override;

private:
  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 scale;

  glm::mat4 BuildModelMatrix() const;

  std::vector<std::shared_ptr<Mesh>> meshes;
};