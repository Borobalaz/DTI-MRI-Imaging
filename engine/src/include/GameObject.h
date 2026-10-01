#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "Mesh.h"
#include "IDrawable.h"
#include "IUpdateable.h"
#include "Shader.h"
#include "Transform.h"
#include "Uniform/UniformProvider.h"
#include "Inspection/InspectField.h"

struct RenderProxy;

class GameObject : public UniformProvider, public IDrawable, public IUpdateable, public InspectProvider
{
public:
  explicit GameObject(const std::string& id);
  GameObject(std::shared_ptr<Geometry> geometry, std::shared_ptr<Material> material, const std::string& id = std::string());
  ~GameObject();

  void AddMesh(std::shared_ptr<Mesh> mesh);
  void SetMaterial(std::shared_ptr<Material> material);

  void BuildRenderProxy(RenderProxy& context) const override;
  void Apply(Shader& shader) const override;

  void SetPosition(const glm::vec3& pos) { transform.SetPosition(pos); }
  void SetRotation(const glm::vec3& rot) { transform.SetRotation(rot); }
  void SetScale(const glm::vec3& s) { transform.SetScale(s); }
  const glm::vec3& GetPosition() const { return transform.GetPosition(); }
  const glm::vec3& GetRotation() const { return transform.GetRotation(); }
  const glm::vec3& GetScale() const { return transform.GetScale(); }
  const std::string& GetId() const { return id; }

  // InspectProvider implementation
  std::vector<InspectFieldPtr> GetInspectFields() override;
  std::optional<float> CastRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const override;
  bool HasVisibility() const override { return true; }
  bool IsVisible() const override { return visible; }
  void SetVisible(bool newVisible) override { visible = newVisible; }
  std::string GetInspectDisplayName() const override;

private:
  Transform transform;
  bool visible = true;

  glm::mat4 BuildModelMatrix() const;

  const std::string id;
  std::vector<std::shared_ptr<Mesh>> meshes;
};
