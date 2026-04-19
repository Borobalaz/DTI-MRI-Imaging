#include "GameObject.h"

#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "Uniform/CompositeUniformProvider.h"
#include "ui/widgets/inspect_fields/InspectCheckboxFieldWidget.h"
#include "ui/widgets/inspect_fields/InspectVec3FieldWidget.h"

GameObject::GameObject(const std::string& id)
  : id(id),
    position(glm::vec3(0.0f)),
    rotation(glm::vec3(0.0f)),
    scale(glm::vec3(1.0f))
{
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

/**
 * @brief You know.
 *
 * @param deltaTime
 */
void GameObject::Update(float deltaTime)
{
}

/**
 * @brief Render the game object's meshes with the provided uniforms.
 *
 * @param frameUniforms
 */
void GameObject::Draw(const UniformProvider &frameUniforms) const
{
  if (meshes.empty() || !visible)
  {
    return;
  }

  CompositeUniformProvider compositeProvider;
  compositeProvider.AddProvider(frameUniforms);
  compositeProvider.AddProvider(*this);

  for (const auto &mesh : meshes)
  {
    if (mesh)
    {
      mesh->Draw(compositeProvider);
    }
  }
}

/**
 * @brief UniformProvider implementation. Set the gameObject uniforms in the shader.
 *
 * @param shader
 */
void GameObject::Apply(Shader &shader) const
{
  const glm::mat4 modelMatrix = BuildModelMatrix();
  shader.SetMat4(ComposeUniformName("gameObject", "modelMatrix"), modelMatrix);
}

/**
 * @brief Construct the model matrix from the position, rotation and scale of the gameObject.
 *
 * @return glm::mat4
 */
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
 * @return std::vector<std::shared_ptr<IInspectWidget>> 
 */
std::vector<std::shared_ptr<IInspectWidget>> GameObject::GetInspectFields()
{
  auto positionField = std::make_shared<InspectVec3FieldWidget>("position", "Position", "Transform");
  positionField->SetValue(QVariantList{position.x, position.y, position.z});
  positionField->valueChangedCallback = [this](const QVariant &value)
  {
    const QVariantList list = value.toList();
    if (list.size() >= 3)
    {
      position = glm::vec3(static_cast<float>(list[0].toDouble()),
                           static_cast<float>(list[1].toDouble()),
                           static_cast<float>(list[2].toDouble()));
    }
  };

  auto rotationField = std::make_shared<InspectVec3FieldWidget>("rotation", "Rotation", "Transform");
  rotationField->SetValue(QVariantList{rotation.x, rotation.y, rotation.z});
  rotationField->valueChangedCallback = [this](const QVariant &value)
  {
    const QVariantList list = value.toList();
    if (list.size() >= 3)
    {
      rotation = glm::vec3(static_cast<float>(list[0].toDouble()),
                           static_cast<float>(list[1].toDouble()),
                           static_cast<float>(list[2].toDouble()));
    }
  };

  auto scaleField = std::make_shared<InspectVec3FieldWidget>("scale", "Scale", "Transform");
  scaleField->SetValue(QVariantList{scale.x, scale.y, scale.z});
  scaleField->valueChangedCallback = [this](const QVariant &value)
  {
    const QVariantList list = value.toList();
    if (list.size() >= 3)
    {
      scale = glm::vec3(static_cast<float>(list[0].toDouble()),
                        static_cast<float>(list[1].toDouble()),
                        static_cast<float>(list[2].toDouble()));
    }
  };

  auto visibleField = std::make_shared<InspectCheckboxFieldWidget>("isVisible", "Visible", "Rendering");
  visibleField->SetValue(visible);
  visibleField->valueChangedCallback = [this](const QVariant &value)
  {
    visible = value.toBool();
  };

  return {positionField, rotationField, scaleField, visibleField};
}


