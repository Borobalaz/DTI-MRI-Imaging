#include "Mat3Volume.h"

#include <vector>

#include "Texture3D.h"

Mat3Volume::Mat3Volume(const VolumeData<glm::mat3>& volumeData,
                       std::shared_ptr<Shader> shader)
  : Volume(volumeData.GetMetadata(), std::move(shader))
{
  const VolumeMetadata& metadata = volumeData.GetMetadata();
  const size_t voxelCount = volumeData.GetVoxelCount();
  std::vector<float> row0(voxelCount * 3);
  std::vector<float> row1(voxelCount * 3);
  std::vector<float> row2(voxelCount * 3);

  for (size_t i = 0; i < voxelCount; ++i)
  {
    const glm::mat3& matrix = volumeData.GetVoxels()[i];
    row0[i * 3 + 0] = matrix[0][0];
    row0[i * 3 + 1] = matrix[1][0];
    row0[i * 3 + 2] = matrix[2][0];
    row1[i * 3 + 0] = matrix[0][1];
    row1[i * 3 + 1] = matrix[1][1];
    row1[i * 3 + 2] = matrix[2][1];
    row2[i * 3 + 0] = matrix[0][2];
    row2[i * 3 + 1] = matrix[1][2];
    row2[i * 3 + 2] = matrix[2][2];
  }

  textureSet.AddTexture(std::make_shared<Texture3D>(
    metadata.dimensions.x,
    metadata.dimensions.y,
    metadata.dimensions.z,
    GL_RGB32F,
    GL_RGB,
    GL_FLOAT,
    row0.data(),
    true));

  textureSet.AddTexture(std::make_shared<Texture3D>(
    metadata.dimensions.x,
    metadata.dimensions.y,
    metadata.dimensions.z,
    GL_RGB32F,
    GL_RGB,
    GL_FLOAT,
    row1.data(),
    true));

  textureSet.AddTexture(std::make_shared<Texture3D>(
    metadata.dimensions.x,
    metadata.dimensions.y,
    metadata.dimensions.z,
    GL_RGB32F,
    GL_RGB,
    GL_FLOAT,
    row2.data(),
    true));
}

const VolumeTextureSet& Mat3Volume::GetTextureSet() const
{
  return textureSet;
}