#include "Volume.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "VolumeTextureSet.h"

namespace
{
  constexpr unsigned int kVolumeTextureBaseUnit = 8;
}

Volume::Volume(const VolumeMetadata& metadata,
               std::shared_ptr<Shader> shader)
  : dimensions(metadata.dimensions),
    spacing(metadata.spacing),
    geometry(std::make_shared<VolumeGeometry>()),
    shader(std::move(shader))
{
}

void Volume::Apply(Shader& shader) const
{
  if (shader.HasUniform("volume.dimensions"))
  {
    shader.SetVec3("volume.dimensions", glm::vec3(dimensions));
  }

  if (shader.HasUniform("volume.spacing"))
  {
    shader.SetVec3("volume.spacing", spacing);
  }

  if (shader.HasUniform("volume.textureCount"))
  {
    shader.SetInt("volume.textureCount", static_cast<int>(GetTextureSet().Size()));
  }
}

void Volume::Draw(const UniformProvider& frameUniforms) const
{
  if (!IsValid() || !visible)
  {
    return;
  }

  GLboolean previousBlendEnabled = glIsEnabled(GL_BLEND);
  GLboolean previousDepthWriteMask = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthWriteMask);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);

  shader->Use();
  frameUniforms.Apply(*shader);
  shader->Apply(*shader);
  Apply(*shader);
  shader->SetMat4("volumeObject.modelMatrix", BuildModelMatrix());
  shader->SetMat4("volumeObject.inverseModelMatrix", glm::inverse(BuildModelMatrix()));
  GetTextureSet().Bind(*shader, kVolumeTextureBaseUnit, "volumeTextures");
  geometry->Draw(*shader);

  if (!previousBlendEnabled)
  {
    glDisable(GL_BLEND);
  }
  glDepthMask(previousDepthWriteMask);
}

bool Volume::IsValid() const
{
  return dimensions.x > 0 && dimensions.y > 0 && dimensions.z > 0 &&
         geometry != nullptr && shader != nullptr && shader->ID != 0 &&
         GetTextureSet().IsValid();
}

glm::mat4 Volume::BuildModelMatrix() const
{
  glm::mat4 model = glm::mat4(1.0f);
  model = glm::translate(model, position);
  model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
  model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
  model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
  model = glm::scale(model, scale);
  return model;
}

void Volume::CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix)
{
  const std::string group = groupPrefix.empty() ? "Volume" : groupPrefix;

  UiField positionField;
  positionField.group = group;
  positionField.label = "Position";
  positionField.kind = UiFieldKind::Vec3;
  positionField.speed = 0.01f;
  positionField.getter = [this]() -> UiFieldValue
  {
    return position;
  };
  positionField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<glm::vec3>(value))
    {
      return;
    }

    position = std::get<glm::vec3>(value);
  };
  out.push_back(std::move(positionField));

  UiField rotationField;
  rotationField.group = group;
  rotationField.label = "Rotation";
  rotationField.kind = UiFieldKind::Vec3;
  rotationField.speed = 0.01f;
  rotationField.getter = [this]() -> UiFieldValue
  {
    return rotation;
  };
  rotationField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<glm::vec3>(value))
    {
      return;
    }

    rotation = std::get<glm::vec3>(value);
  };
  out.push_back(std::move(rotationField));

  UiField scaleField;
  scaleField.group = group;
  scaleField.label = "Scale";
  scaleField.kind = UiFieldKind::Vec3;
  scaleField.speed = 0.01f;
  scaleField.getter = [this]() -> UiFieldValue
  {
    return scale;
  };
  scaleField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<glm::vec3>(value))
    {
      return;
    }

    scale = std::get<glm::vec3>(value);
  };
  out.push_back(std::move(scaleField));

  UiField visibleField;
  visibleField.group = group;
  visibleField.label = "Visible";
  visibleField.kind = UiFieldKind::Bool;
  visibleField.getter = [this]() -> UiFieldValue
  {
    return visible;
  };
  visibleField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<bool>(value))
    {
      return;
    }

    visible = std::get<bool>(value);
  };
  out.push_back(std::move(visibleField));


}

void Volume::CollectInspectableNodes(std::vector<InspectableNode>& out, const std::string& nodePrefix)
{
  // First, collect the volume's own fields
  IInspectable::CollectInspectableNodes(out, nodePrefix);

  // Then, add the shader as a nested inspectable node
  if (shader)
  {
    InspectableNode shaderNode;
    shaderNode.nodeLabel = "Shader";
    shaderNode.isField = false;
    shaderNode.nestedInspectable = std::static_pointer_cast<IInspectable>(shader);
    out.push_back(shaderNode);
  }
}
