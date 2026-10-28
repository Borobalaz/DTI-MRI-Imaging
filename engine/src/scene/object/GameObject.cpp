#include "scene/object/GameObject.h"

#include <algorithm>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

#include "scene/extraction/RenderProxy.h"
#include "render/uniform/CompositeUniformProvider.h"

GameObject::GameObject(const std::string& id)
  : id(id)
{
}

GameObject::GameObject(std::shared_ptr<Geometry> geometry, std::shared_ptr<Material> material, const std::string& id)
  : id(id)
{
  if (geometry && material)
  {
    auto mesh = std::make_shared<Mesh>(geometry, material);
    AddMesh(mesh);
  }
}

GameObject::~GameObject()
{
}

/**
 * @brief Push back a mesh on the game object.
 *
 * @param mesh
 */
void GameObject::AddMesh(std::shared_ptr<Mesh> mesh)
{
  if (!mesh)
  {
    return;
  }

  meshes.push_back(std::move(mesh));
}

void GameObject::SetMaterial(std::shared_ptr<Material> material)
{
  if (!material)
  {
    return;
  }

  for (const auto& mesh : meshes)
  {
    if (mesh)
    {
      mesh->SetMaterial(material);
    }
  }
}

/**
 * @brief Render the game object's meshes with the provided uniforms.
 *
 * @param frameUniforms
 */
void GameObject::BuildRenderProxy(RenderProxy& context) const
{
  if (meshes.empty() || !visible)
  {
    context.visible = false;
    return;
  }

  if (IsInstanced())
  {
    BuildInstancedRenderProxy(context);
    return;
  }

  context.visible = true;
  MeshRenderBatch batch;
  for (const auto& mesh : meshes)
  {
    if (!mesh)
    {
      continue;
    }

    const std::shared_ptr<Geometry> geometry = mesh->GetGeometry();
    const std::shared_ptr<Material> material = mesh->GetMaterial();
    if (!geometry || !material)
    {
      continue;
    }

    MeshRenderCommand command;
    command.geometry = geometry;
    command.material = material;
    command.modelMatrix = BuildModelMatrix();
    command.frameUniforms = context.frameUniforms;
    batch.draws.push_back(std::move(command));
  }

  context.visible = !batch.draws.empty();
  if (context.visible)
  {
    context.data.Append(std::move(batch));
  }
}

/**
 * @brief Set the per-instance transform list this GameObject renders with. An
 *        empty list clears instancing (see ClearInstanceTransforms).
 *
 * @param transforms per-instance model matrices
 */
void GameObject::SetInstanceTransforms(const std::vector<glm::mat4>& transforms)
{
  instanceBuffer.SetTransforms(transforms);
}

/**
 * @brief Clear any instance transforms, returning this GameObject to the
 *        normal single-draw rendering path.
 */
void GameObject::ClearInstanceTransforms()
{
  instanceBuffer.Clear();
}

/**
 * @brief Build a MeshInstancedRenderCommand per mesh, one shared instance
 *        buffer across all of them. This GameObject's own transform is used
 *        as a shared outer/group transform applied to every instance (see
 *        MeshInstancedRenderCommand::modelMatrix) - e.g. so SetPosition/
 *        SetRotation can move or spin the whole instanced batch as a group,
 *        while each instance's own matrix stays a local offset within it.
 *
 * @param context
 */
void GameObject::BuildInstancedRenderProxy(RenderProxy& context) const
{
  context.visible = true;
  MeshInstancedRenderBatch batch;
  for (const auto& mesh : meshes)
  {
    if (!mesh)
    {
      continue;
    }

    const std::shared_ptr<Geometry> geometry = mesh->GetGeometry();
    const std::shared_ptr<Material> material = mesh->GetMaterial();
    if (!geometry || !material)
    {
      continue;
    }

    MeshInstancedRenderCommand command;
    command.geometry = geometry;
    command.material = material;
    command.instances = &instanceBuffer;
    command.modelMatrix = BuildModelMatrix();
    command.frameUniforms = context.frameUniforms;
    batch.draws.push_back(std::move(command));
  }

  context.visible = !batch.draws.empty();
  if (context.visible)
  {
    context.data.Append(std::move(batch));
  }
}

/**
 * @brief UniformProvider implementation. Set the gameObject uniforms in the shader.
 *
 * @param shader
 */
void GameObject::Apply(IUniformSink &shader) const
{
  const glm::mat4 modelMatrix = BuildModelMatrix();
  shader.SetMat4(ComposeUniformName("gameObject", "modelMatrix"), modelMatrix);
}

/**
 * @brief Construct the model matrix from the transform of the gameObject.
 *
 * @return glm::mat4
 */
glm::mat4 GameObject::BuildModelMatrix() const
{
  return transform.GetModelMatrix();
}

/**
 * @brief Get the string which identifies the object. 
 * 
 * @return std::string 
 */
std::string GameObject::GetInspectDisplayName() const
{
  return id.empty() ? std::string("Game Object") : id;
}

/**
 * @brief Get the fields that should be visible in the inspector: position, rotation, scale, and visibility.
 * 
 * @return engine inspection fields
 */
std::vector<InspectFieldPtr> GameObject::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields = transform.GetInspectFields();

  auto visibleField = MakeInspectField("isVisible", "Visible", "Rendering", InspectFieldType::Boolean, visible,
    [this]() -> InspectValue { return visible; },
    [this](const InspectValue &value)
  {
    if (const auto *boolean = std::get_if<bool>(&value))
      visible = *boolean;
  });

  fields.push_back(visibleField);

  if (IsInstanced())
  {
    auto instanceCountField = MakeInspectField(
      "instanceCount", "Instance Count", "Rendering", InspectFieldType::Number,
      static_cast<double>(GetInstanceCount()));
    instanceCountField->readOnly = true;
    fields.push_back(instanceCountField);
  }

  return fields;
}

/**
 * @brief Cast a ray against the gameObject and return the distance to the intersection point.
 *        Use a bounding sphere for intersection test, with the radius being the maximum scale component.
 * 
 * @param rayOrigin 
 * @param rayDirection 
 * @return std::optional<float> 
 */
std::optional<float> GameObject::CastRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const
{
  if (!visible)
  {
    return std::nullopt;
  }

  const float radius = std::max({std::abs(transform.GetScale().x), std::abs(transform.GetScale().y), std::abs(transform.GetScale().z), 0.1f});
  const glm::vec3 toCenter = rayOrigin - transform.GetPosition();

  const float a = glm::dot(rayDirection, rayDirection);
  const float b = 2.0f * glm::dot(toCenter, rayDirection);
  const float c = glm::dot(toCenter, toCenter) - radius * radius;
  const float discriminant = b * b - 4.0f * a * c;

  if (discriminant < 0.0f)
  {
    return std::nullopt;
  }

  const float sqrtDiscriminant = std::sqrt(discriminant);
  const float t0 = (-b - sqrtDiscriminant) / (2.0f * a);
  const float t1 = (-b + sqrtDiscriminant) / (2.0f * a);

  if (t0 >= 0.0f)
  {
    return t0;
  }

  if (t1 >= 0.0f)
  {
    return t1;
  }

  return std::nullopt;
}

