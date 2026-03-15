#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "IDrawable.h"
#include "Shader.h"
#include "UniformProvider.h"
#include "VolumeGeometry.h"
#include "VolumeGpuEncoder.h"
#include "VolumeRenderSettings.h"

class VolumeObject : public UniformProvider, IDrawable
{
public:
  VolumeObject(EncodedVolumeResource encodedVolume,
               const VolumeRenderSettings& renderSettings);

  template <typename TVoxel>
  static std::shared_ptr<VolumeObject> Create(
    const VolumeData<TVoxel>& volumeData,
    const VolumeRenderSettings& renderSettings = VolumeRenderSettings());

  static std::shared_ptr<VolumeObject> CreateFromFile(
    const std::string& filePath,
    const VolumeRenderSettings& renderSettings = VolumeRenderSettings());

  void Apply(Shader& shader) const override;
  void Draw(const UniformProvider& frameUniforms) const override;
  bool IsValid() const;

  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 rotation{0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};

private:
  glm::mat4 BuildModelMatrix() const;

  EncodedVolumeResource encodedVolume;
  VolumeRenderSettings renderSettings;
  std::shared_ptr<VolumeGeometry> geometry;
  std::shared_ptr<Shader> shader;
};

template <typename TVoxel>
std::shared_ptr<VolumeObject> VolumeObject::Create(
  const VolumeData<TVoxel>& volumeData,
  const VolumeRenderSettings& renderSettings)
{
  EncodedVolumeResource encodedVolume = VolumeGpuEncoder<TVoxel>::Encode(volumeData);
  if (!encodedVolume.IsValid())
  {
    return nullptr;
  }

  return std::make_shared<VolumeObject>(std::move(encodedVolume), renderSettings);
}
