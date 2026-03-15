#include "VolumeObject.h"

#include <glm/gtc/matrix_transform.hpp>

#include "VolumeFileLoader.h"

namespace
{
  constexpr unsigned int kVolumeTextureBaseUnit = 8;
}

std::shared_ptr<VolumeObject> VolumeObject::CreateFromFile(
  const std::string& filePath,
  const VolumeRenderSettings& renderSettings)
{
  const std::optional<LoadedVolumeVariant> loadedVolume = VolumeFileLoader::Load(filePath);
  if (!loadedVolume.has_value())
  {
    return nullptr;
  }

  return std::visit(
    [&renderSettings](const auto& typedVolume)
    {
      return VolumeObject::Create(typedVolume, renderSettings);
    },
    *loadedVolume);
}

VolumeObject::VolumeObject(EncodedVolumeResource encodedVolume,
                           const VolumeRenderSettings& renderSettings)
  : encodedVolume(std::move(encodedVolume)),
    renderSettings(renderSettings),
    geometry(std::make_shared<VolumeGeometry>()),
    shader(std::make_shared<Shader>("shaders/volume_vertex.glsl", "shaders/volume_fragment.glsl"))
{
}

void VolumeObject::Draw(const UniformProvider& frameUniforms) const
{
  if (!IsValid())
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
  Apply(*shader);
  shader->SetMat4("volumeObject.modelMatrix", BuildModelMatrix());
  shader->SetMat4("volumeObject.inverseModelMatrix", glm::inverse(BuildModelMatrix()));
  encodedVolume.textureSet.Bind(*shader, kVolumeTextureBaseUnit, "volumeTextures");
  geometry->Draw(*shader);

  if (!previousBlendEnabled)
  {
    glDisable(GL_BLEND);
  }
  glDepthMask(previousDepthWriteMask);
}

void VolumeObject::Apply(Shader& shader) const
{
  if (shader.HasUniform("volume.dimensions"))
  {
    shader.SetVec3("volume.dimensions", glm::vec3(encodedVolume.dimensions));
  }

  if (shader.HasUniform("volume.spacing"))
  {
    shader.SetVec3("volume.spacing", encodedVolume.spacing);
  }

  if (shader.HasUniform("volume.stepSize"))
  {
    shader.SetFloat("volume.stepSize", renderSettings.stepSize);
  }

  if (shader.HasUniform("volume.opacityScale"))
  {
    shader.SetFloat("volume.opacityScale", renderSettings.opacityScale);
  }

  if (shader.HasUniform("volume.intensityScale"))
  {
    shader.SetFloat("volume.intensityScale", renderSettings.intensityScale);
  }

  if (shader.HasUniform("volume.threshold"))
  {
    shader.SetFloat("volume.threshold", renderSettings.threshold);
  }

  if (shader.HasUniform("volume.maxSteps"))
  {
    shader.SetInt("volume.maxSteps", renderSettings.maxSteps);
  }

  if (shader.HasUniform("volume.textureCount"))
  {
    shader.SetInt("volume.textureCount", static_cast<int>(encodedVolume.textureSet.Size()));
  }

  if (shader.HasUniform("volume.voxelKind"))
  {
    shader.SetInt("volume.voxelKind", static_cast<int>(encodedVolume.voxelKind));
  }

  if (shader.HasUniform("volume.renderMode"))
  {
    shader.SetInt("volume.renderMode", static_cast<int>(renderSettings.renderMode));
  }

  if (shader.HasUniform("volume.matrixRows"))
  {
    shader.SetInt("volume.matrixRows", encodedVolume.matrixRows);
  }

  if (shader.HasUniform("volume.matrixCols"))
  {
    shader.SetInt("volume.matrixCols", encodedVolume.matrixCols);
  }
}

bool VolumeObject::IsValid() const
{
  return encodedVolume.IsValid() && geometry != nullptr && shader != nullptr && shader->ID != 0;
}

glm::mat4 VolumeObject::BuildModelMatrix() const
{
  glm::mat4 model = glm::mat4(1.0f);
  model = glm::translate(model, position);
  model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
  model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
  model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
  model = glm::scale(model, scale);
  return model;
}
