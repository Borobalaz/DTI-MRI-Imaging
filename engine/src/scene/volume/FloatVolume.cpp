#include "scene/volume/FloatVolume.h"

#include "render/texture/Texture3D.h"

FloatVolume::FloatVolume(const std::string& id,
                         const VolumeData& volumeData,
                         std::shared_ptr<Shader> shader)
  : Volume(id, volumeData.GetDimensions(), volumeData.GetSpacing(), std::move(shader))
{
  const glm::ivec3& dimensions = volumeData.GetDimensions();
  textureSet.AddTexture(std::make_shared<Texture3D>(
    dimensions.x,
    dimensions.y,
    dimensions.z,
    TextureFormat::R32F,
    volumeData.GetVoxels().data(),
    true));
}
