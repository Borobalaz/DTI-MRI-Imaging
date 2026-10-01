#include "Volume/Volume.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

#include "Renderer/RenderProxy.h"
#include "Volume/VolumeTextureSet.h"

/**
 * @brief Construct a new Volume:: Volume object
 *
 * @param metadata
 * @param shader
 */
Volume::Volume(const std::string &id,
               const glm::ivec3 &dimensions,
               const glm::vec3 &spacing,
               std::shared_ptr<Shader> volumeShader)
    : id(id),
      dimensions(dimensions),
      spacing(spacing),
      geometry(std::make_shared<VolumeGeometry>()),
      shader(std::move(volumeShader))
{
  const glm::vec3 physicalExtents = glm::vec3(dimensions) * spacing;
  const float maxExtent = std::max({physicalExtents.x, physicalExtents.y, physicalExtents.z, 1e-6f});
  transform.SetScale(physicalExtents / maxExtent);
}


/**
 * @brief Apply the volume's uniform values to the shader.
 *
 * @param shader The shader to apply the uniforms to.
 */
void Volume::Apply(Shader &shader) const
{
  shader.SetVec3("volume.dimensions", glm::vec3(dimensions));
  shader.SetInt("volume.textureCount", static_cast<int>(GetTextureSet().Size()));
}

void Volume::BuildRenderProxy(RenderProxy& renderProxy) const
{
  if (!IsValid() || !visible)
  {
    renderProxy.visible = false;
    return;
  }

  renderProxy.visible = true;
  auto textures = std::make_shared<VolumeTextureSet>(textureSet);
  CompositeUniformProvider volumeUniforms = renderProxy.frameUniforms;
  volumeUniforms.AddProvider(*this);

  renderProxy.data.Append(VolumeRenderCommand{
    geometry,
    shader,
    std::move(textures),
    dimensions,
    BuildModelMatrix(),
    glm::inverse(BuildModelMatrix()),
    std::move(volumeUniforms)});
}

/**
 * @brief Draw the volume using the provided uniform values.
 *
 * @param frameUniforms The uniform values for the current frame.
 */
void Volume::Draw(const UniformProvider &frameUniforms) const
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
  shader->SetMat4("volumeObject.modelMatrix", modelMatrix);
  shader->SetMat4("volumeObject.inverseModelMatrix", inverseModelMatrix);
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

std::vector<InspectFieldPtr> Volume::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields = transform.GetInspectFields();

  auto visibleField = MakeInspectField("visible", "Visible", "Rendering", InspectFieldType::Boolean, visible,
    [this]() -> InspectValue { return visible; },
    [this](const InspectValue &value)
  {
    if (const auto *boolean = std::get_if<bool>(&value)) visible = *boolean;
  });
  fields.push_back(visibleField);

  if (!shader)
  {
    return fields;
  }

  // Add fields for each stored uniform in the shader
  const std::map<std::string, Shader::UniformValue> &uniforms = shader->GetStoredUniforms();
  for (const auto &[uniformName, uniformValue] : uniforms)
  {
    std::string displayName = uniformName;
    constexpr const char *shaderPrefix = "shader.";
    if (displayName.rfind(shaderPrefix, 0) == 0)
    {
      displayName = displayName.substr(7);
    }

    const std::string fieldId = "uniform." + uniformName;

    if (std::holds_alternative<bool>(uniformValue))
    {
      auto field = MakeInspectField(fieldId, displayName, "Shader Uniforms", InspectFieldType::Boolean, std::get<bool>(uniformValue),
        [this, uniformName]() -> InspectValue
        {
          const auto &values = shader->GetStoredUniforms();
          const auto it = values.find(uniformName);
          return it != values.end() && std::holds_alternative<bool>(it->second) ? InspectValue{std::get<bool>(it->second)} : InspectValue{false};
        },
        [this, uniformName](const InspectValue &value)
      {
        if (const auto *boolean = std::get_if<bool>(&value)) (*shader)[uniformName] = *boolean;
      });
      fields.push_back(field);
      continue;
    }

    if (std::holds_alternative<int>(uniformValue))
    {
      auto field = MakeInspectField(fieldId, displayName, "Shader Uniforms", InspectFieldType::Number, static_cast<double>(std::get<int>(uniformValue)),
          [this, uniformName]() -> InspectValue
          {
            const auto &values = shader->GetStoredUniforms();
            const auto it = values.find(uniformName);
            return it != values.end() && std::holds_alternative<int>(it->second) ? InspectValue{static_cast<double>(std::get<int>(it->second))} : InspectValue{0.0};
          },
          [this, uniformName](const InspectValue &value)
          {
            if (const auto *number = std::get_if<double>(&value)) (*shader)[uniformName] = static_cast<int>(*number);
          });
      field->minimum = -1e9;
      field->maximum = 1e9;
      field->step = 1.0;
      fields.push_back(field);
      continue;
    }

    if (std::holds_alternative<float>(uniformValue))
    {
      auto field = MakeInspectField(fieldId, displayName, "Shader Uniforms", InspectFieldType::Number, static_cast<double>(std::get<float>(uniformValue)),
          [this, uniformName]() -> InspectValue
          {
            const auto &values = shader->GetStoredUniforms();
            const auto it = values.find(uniformName);
            return it != values.end() && std::holds_alternative<float>(it->second) ? InspectValue{static_cast<double>(std::get<float>(it->second))} : InspectValue{0.0};
          },
          [this, uniformName](const InspectValue &value)
          {
            if (const auto *number = std::get_if<double>(&value)) (*shader)[uniformName] = static_cast<float>(*number);
          });
      field->minimum = -1e9;
      field->maximum = 1e9;
      field->step = 0.01;
      fields.push_back(field);
      continue;
    }

    if (std::holds_alternative<glm::vec3>(uniformValue))
    {
      auto field = MakeInspectField(fieldId, displayName, "Shader Uniforms", InspectFieldType::Vector3, std::get<glm::vec3>(uniformValue),
        [this, uniformName]() -> InspectValue
        {
          const auto &values = shader->GetStoredUniforms();
          const auto it = values.find(uniformName);
          return it != values.end() && std::holds_alternative<glm::vec3>(it->second) ? InspectValue{std::get<glm::vec3>(it->second)} : InspectValue{glm::vec3(0.0f)};
        },
        [this, uniformName](const InspectValue &value)
      {
        if (const auto *vector = std::get_if<glm::vec3>(&value)) (*shader)[uniformName] = *vector;
      });
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
  return transform.GetModelMatrix();
}

/**
 * @brief Cast a ray against the volume and return the distance to the intersection point.
 *        Use a bounding box for intersection test, with the box defined from (-0.5, -0.5, -0.5) to (0.5, 0.5, 0.5) in local space.
 *
 * @param rayOrigin
 * @param rayDirection
 * @return std::optional<float>
 */
std::optional<float> Volume::CastRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const
{
  if (!visible)
  {
    return std::nullopt;
  }

  const glm::mat4 inverseModel = glm::inverse(BuildModelMatrix());
  const glm::vec3 localOrigin = glm::vec3(inverseModel * glm::vec4(rayOrigin, 1.0f));
  const glm::vec3 localDirection = glm::normalize(glm::vec3(inverseModel * glm::vec4(rayDirection, 0.0f)));

  constexpr glm::vec3 boxMin(-0.5f, -0.5f, -0.5f);
  constexpr glm::vec3 boxMax(0.5f, 0.5f, 0.5f);

  float tMin = 0.0f;
  float tMax = std::numeric_limits<float>::max();

  for (int axis = 0; axis < 3; ++axis)
  {
    const float originComponent = localOrigin[axis];
    const float directionComponent = localDirection[axis];

    if (std::abs(directionComponent) < 1e-6f)
    {
      if (originComponent < boxMin[axis] || originComponent > boxMax[axis])
      {
        return std::nullopt;
      }
      continue;
    }

    const float inverseDirection = 1.0f / directionComponent;
    float t1 = (boxMin[axis] - originComponent) * inverseDirection;
    float t2 = (boxMax[axis] - originComponent) * inverseDirection;
    if (t1 > t2)
    {
      std::swap(t1, t2);
    }

    tMin = std::max(tMin, t1);
    tMax = std::min(tMax, t2);

    if (tMin > tMax)
    {
      return std::nullopt;
    }
  }

  if (tMax < 0.0f)
  {
    return std::nullopt;
  }

  const float localHit = tMin >= 0.0f ? tMin : tMax;
  const glm::vec3 localHitPoint = localOrigin + localDirection * localHit;
  const glm::vec3 worldHitPoint = glm::vec3(BuildModelMatrix() * glm::vec4(localHitPoint, 1.0f));
  return glm::length(worldHitPoint - rayOrigin);
}
