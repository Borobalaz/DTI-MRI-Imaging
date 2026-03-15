#include "GameObject.h"

#include <glm/gtc/matrix_transform.hpp>

#include "CompositeUniformProvider.h"

GameObject::GameObject()
{
  position = glm::vec3(0.0f);
  rotation = glm::vec3(0.0f);
  scale = glm::vec3(1.0f);
}

GameObject::~GameObject()
{
}

void GameObject::AddMesh(std::shared_ptr<Mesh> mesh)
{
  if (!mesh)
  {
    return;
  }

  meshes.push_back(std::move(mesh));
}

void GameObject::Update(float deltaTime)
{
}

void GameObject::Draw(const UniformProvider& frameUniforms) const
{
  if (meshes.empty())
  {
    return;
  }

  CompositeUniformProvider compositeProvider;
  compositeProvider.AddProvider(frameUniforms);
  compositeProvider.AddProvider(*this);

  for (const auto& mesh : meshes)
  {
    if (mesh)
    {
      mesh->Draw(compositeProvider);
    }
  }
}

void GameObject::Apply(Shader& shader) const
{
  const glm::mat4 modelMatrix = BuildModelMatrix();
  shader.SetMat4(ComposeUniformName("gameObject", "modelMatrix"), modelMatrix);
}

glm::mat4 GameObject::BuildModelMatrix() const
{
  glm::mat4 model = glm::mat4(1.0f);
  model = glm::translate(model, position);
  model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
  model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
  model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
  model = glm::scale(model, scale);
  return model;
}