#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "Mesh.h"
#include "IDrawable.h"
#include "IUpdateable.h"
#include "Render/Shader.h"
#include "Transform.h"
#include "Render/Uniform/UniformProvider.h"
#include "Inspection/InspectField.h"
#include "Render/Geometry/InstanceBuffer.h"

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
  void Apply(IUniformSink& shader) const override;

  void SetPosition(const glm::vec3& pos) { transform.SetPosition(pos); }
  void SetRotation(const glm::vec3& rot) { transform.SetRotation(rot); }
  void SetScale(const glm::vec3& s) { transform.SetScale(s); }
  const glm::vec3& GetPosition() const { return transform.GetPosition(); }
  const glm::vec3& GetRotation() const { return transform.GetRotation(); }
  const glm::vec3& GetScale() const { return transform.GetScale(); }
  const std::string& GetId() const { return id; }

  // GPU instancing: when a non-empty transform list is set, BuildRenderProxy
  // emits one MeshInstancedRenderCommand per mesh instead of the regular
  // MeshRenderCommand. This GameObject's own transform (SetPosition/
  // SetRotation/SetScale) is NOT ignored - it's applied as a shared outer/
  // group transform on top of every instance (final = ownTransform *
  // instanceMatrix), so the whole batch can be moved or animated as a group
  // (e.g. per-frame rotation) while each instance matrix stays a local offset
  // within that group. SetInstanceTransforms with an empty vector is
  // equivalent to ClearInstanceTransforms(). Must be called from engine/GL-
  // thread code, same constraint as any call that creates GL resources in
  // this engine. The material assigned to an instanced GameObject must use a
  // vertex shader that reads the instance attributes (locations 4-7, e.g.
  // vertex_instanced.glsl) - otherwise no model matrix is applied and the
  // object renders collapsed/degenerate.
  void SetInstanceTransforms(const std::vector<glm::mat4>& transforms);
  void ClearInstanceTransforms();
  bool IsInstanced() const { return instanceBuffer.IsValid(); }
  size_t GetInstanceCount() const { return instanceBuffer.GetInstanceCount(); }

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
  void BuildInstancedRenderProxy(RenderProxy& context) const;

  const std::string id;
  std::vector<std::shared_ptr<Mesh>> meshes;
  InstanceBuffer instanceBuffer;
};
