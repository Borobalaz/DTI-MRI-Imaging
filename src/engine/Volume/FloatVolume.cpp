#include "FloatVolume.h"

#include "Texture3D.h"

FloatVolume::FloatVolume(const VolumeData& volumeData,
                         std::shared_ptr<Shader> shader)
  : Volume(volumeData.GetDimensions(), volumeData.GetSpacing(), std::move(shader))
{
  const glm::ivec3& dimensions = volumeData.GetDimensions();
  textureSet.AddTexture(std::make_shared<Texture3D>(
    dimensions.x,
    dimensions.y,
    dimensions.z,
    GL_R32F,
    GL_RED,
    GL_FLOAT,
    volumeData.GetVoxels().data(),
    true));
}
