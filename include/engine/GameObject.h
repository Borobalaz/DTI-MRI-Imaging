#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "Mesh.h"
#include "IDrawable.h"
#include "Shader.h"
#include "Uniform/UniformProvider.h"
#include "ui/widgets/inspect_fields/InspectProvider.h"
#include "ui/widgets/inspect_fields/IInspectWidget.h"

class GameObject : public UniformProvider, public IDrawable, public InspectProvider
{
public:
  explicit GameObject(const std::string& id);
  ~GameObject();

  void AddMesh(std::shared_ptr<Mesh> mesh);

  void Update(float deltaTime);
  void Draw(const UniformProvider& frameUniforms) const override;
  void Apply(Shader& shader) const override;

  void SetPosition(const glm::vec3& pos) { position = pos; }
  void SetRotation(const glm::vec3& rot) { rotation = rot; }
  void SetScale(const glm::vec3& s) { scale = s; }
  const glm::vec3& GetPosition() const { return position; }
  const glm::vec3& GetRotation() const { return rotation; }
  const glm::vec3& GetScale() const { return scale; }
  const std::string& GetId() const { return id; }

  // InspectProvider implementation
  std::vector<std::shared_ptr<IInspectWidget>> GetInspectFields() override;
  std::optional<float> CastRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const override;
  bool HasVisibility() const override { return true; }
  bool IsVisible() const override { return visible; }
  std::string GetInspectDisplayName() const override;

private:
  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 scale;
  bool visible = true;

  glm::mat4 BuildModelMatrix() const;

  const std::string id;
  std::vector<std::shared_ptr<Mesh>> meshes;
};
