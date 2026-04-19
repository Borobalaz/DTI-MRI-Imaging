#include "Volume/Volume.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "Volume/VolumeTextureSet.h"
#include "ui/widgets/inspect_fields/InspectCheckboxFieldWidget.h"
#include "ui/widgets/inspect_fields/InspectNumberFieldWidget.h"
#include "ui/widgets/inspect_fields/InspectVec3FieldWidget.h"

/**
 * @brief Construct a new Volume:: Volume object
 * 
 * @param metadata 
 * @param shader 
 */
Volume::Volume(const std::string& id,
               const glm::ivec3& dimensions,
               const glm::vec3& spacing,
               std::shared_ptr<Shader> shader)
  : id(id),
    dimensions(dimensions),
    spacing(spacing),
    geometry(std::make_shared<VolumeGeometry>()),
    shader(std::move(shader))
{
  const glm::vec3 physicalExtents = glm::vec3(dimensions) * spacing;
  const float maxExtent = std::max({physicalExtents.x, physicalExtents.y, physicalExtents.z, 1e-6f});
  scale = physicalExtents / maxExtent;
}

/**
 * @brief Apply the volume's uniform values to the shader.
 * 
 * @param shader The shader to apply the uniforms to.
 */
void Volume::Apply(Shader& shader) const
{
  if (shader.HasUniform("volume.dimensions"))
  {
    shader.SetVec3("volume.dimensions", glm::vec3(dimensions));
  }

  if (shader.HasUniform("volume.textureCount"))
  {
    shader.SetInt("volume.textureCount", static_cast<int>(GetTextureSet().Size()));
  }
}

/**
 * @brief Draw the volume using the provided uniform values.
 * 
 * @param frameUniforms The uniform values for the current frame.
 */
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
  const glm::mat4 modelMatrix = BuildModelMatrix();
  const glm::mat4 inverseModelMatrix = glm::inverse(modelMatrix);
  if (shader->HasUniform("volumeObject.modelMatrix"))
  {
    shader->SetMat4("volumeObject.modelMatrix", modelMatrix);
  }
  if (shader->HasUniform("volumeObject.inverseModelMatrix"))
  {
    shader->SetMat4("volumeObject.inverseModelMatrix", inverseModelMatrix);
  }
  GetTextureSet().Bind(*shader, "volumeTextures");
  geometry->Draw(*shader);

  if (!previousBlendEnabled)
  {
    glDisable(GL_BLEND);
  }
  glDepthMask(previousDepthWriteMask);
}

/**
 * @brief Check if the volume is valid based on the presence of geometry, shader, and valid dimensions and textures. 
 * 
 * @return True if the volume is valid, false otherwise.
 */
bool Volume::IsValid() const
{
  return dimensions.x > 0 && dimensions.y > 0 && dimensions.z > 0 &&
         geometry != nullptr && shader != nullptr && shader->ID != 0 &&
         GetTextureSet().IsValid();
}

std::string Volume::GetInspectDisplayName() const
{
  return id.empty() ? std::string("Volume") : id;
}

std::vector<std::shared_ptr<IInspectWidget>> Volume::GetInspectFields()
{
  std::vector<std::shared_ptr<IInspectWidget>> fields;

  // Visible checkbox
  auto visibleField = std::make_shared<InspectCheckboxFieldWidget>("visible", "Visible", "Rendering");
  visibleField->SetValue(visible);
  visibleField->valueChangedCallback = [this](const QVariant &value)
  {
    visible = value.toBool();
  };
  fields.push_back(visibleField);

  if (!shader)
  {
    return fields;
  }

  // Add fields for each stored uniform in the shader
  const std::map<std::string, Shader::UniformValue>& uniforms = shader->GetStoredUniforms();
  for (const auto& [uniformName, uniformValue] : uniforms)
  {
    std::string displayName = uniformName;
    constexpr const char* shaderPrefix = "shader.";
    if (displayName.rfind(shaderPrefix, 0) == 0)
    {
      displayName = displayName.substr(7);
    }

    const QString fieldId = QStringLiteral("uniform.") + QString::fromStdString(uniformName);
    const QString fieldDisplayName = QString::fromStdString(displayName);

    if (std::holds_alternative<bool>(uniformValue))
    {
      auto field = std::make_shared<InspectCheckboxFieldWidget>(
          fieldId,
          fieldDisplayName,
          "Shader Uniforms");
      field->SetValue(std::holds_alternative<bool>(uniformValue) ? std::get<bool>(uniformValue) : false);
      field->valueChangedCallback = [this, uniformName](const QVariant &value)
      {
        (*shader)[uniformName] = value.toBool();
      };
      fields.push_back(field);
      continue;
    }

    if (std::holds_alternative<int>(uniformValue))
    {
      fields.push_back(std::make_shared<InspectNumberFieldWidget>(
          fieldId,
          fieldDisplayName,
          "Shader Uniforms",
          [this, uniformName]()
          {
            const auto& values = shader->GetStoredUniforms();
            const auto it = values.find(uniformName);
            return it != values.end() && std::holds_alternative<int>(it->second)
              ? static_cast<double>(std::get<int>(it->second))
              : 0.0;
          },
          [this, uniformName](double value)
          {
            (*shader)[uniformName] = static_cast<int>(value);
          },
          -1e9,
          1e9));
      continue;
    }

    if (std::holds_alternative<float>(uniformValue))
    {
      fields.push_back(std::make_shared<InspectNumberFieldWidget>(
          fieldId,
          fieldDisplayName,
          "Shader Uniforms",
          [this, uniformName]()
          {
            const auto& values = shader->GetStoredUniforms();
            const auto it = values.find(uniformName);
            return it != values.end() && std::holds_alternative<float>(it->second)
              ? static_cast<double>(std::get<float>(it->second))
              : 0.0;
          },
          [this, uniformName](double value)
          {
            (*shader)[uniformName] = static_cast<float>(value);
          },
          -1e9,
          1e9));
      continue;
    }

    if (std::holds_alternative<glm::vec3>(uniformValue))
    {
      auto field = std::make_shared<InspectVec3FieldWidget>(
          fieldId,
          fieldDisplayName,
          "Shader Uniforms");
      field->SetValue(QVariantList{std::get<glm::vec3>(uniformValue).x,
                                   std::get<glm::vec3>(uniformValue).y,
                                   std::get<glm::vec3>(uniformValue).z});
      field->valueChangedCallback = [this, uniformName](const QVariant &value)
      {
        const QVariantList list = value.toList();
        if (list.size() >= 3)
        {
          (*shader)[uniformName] = glm::vec3(static_cast<float>(list[0].toDouble()),
                                            static_cast<float>(list[1].toDouble()),
                                            static_cast<float>(list[2].toDouble()));
        }
      };
      fields.push_back(field);
    }
  }

  return fields;
}

/**
 * @brief Build the model matrix for the volume.
 * 
 * @return The model matrix.
 */
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

