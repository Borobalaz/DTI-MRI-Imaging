#include "FloatVolume.h"

#include "Texture3D.h"

FloatVolume::FloatVolume(const VolumeData<float>& volumeData,
                         std::shared_ptr<Shader> shader)
  : Volume(volumeData.GetMetadata(), std::move(shader))
{
  const VolumeMetadata& metadata = volumeData.GetMetadata();
  textureSet.AddTexture(std::make_shared<Texture3D>(
    metadata.dimensions.x,
    metadata.dimensions.y,
    metadata.dimensions.z,
    GL_R32F,
    GL_RED,
    GL_FLOAT,
    volumeData.GetVoxels().data(),
    true));
}

const VolumeTextureSet& FloatVolume::GetTextureSet() const
{
  return textureSet;
}