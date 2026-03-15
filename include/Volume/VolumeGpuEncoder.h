#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Texture3D.h"
#include "VolumeData.h"
#include "VolumeTextureSet.h"

struct EncodedVolumeResource
{
  VolumeTextureSet textureSet;
  glm::ivec3 dimensions{0, 0, 0};
  glm::vec3 spacing{1.0f, 1.0f, 1.0f};
  VolumeVoxelKind voxelKind{VolumeVoxelKind::Float32};
  int matrixRows{1};
  int matrixCols{1};

  bool IsValid() const
  {
    return dimensions.x > 0 && dimensions.y > 0 && dimensions.z > 0 && textureSet.IsValid();
  }
};

template <typename TVoxel>
struct VolumeGpuEncoder;

template <>
struct VolumeGpuEncoder<uint8_t>
{
  static EncodedVolumeResource Encode(const VolumeData<uint8_t>& volume)
  {
    EncodedVolumeResource encoded;
    const VolumeMetadata& metadata = volume.GetMetadata();
    encoded.dimensions = metadata.dimensions;
    encoded.spacing = metadata.spacing;
    encoded.voxelKind = metadata.voxelKind;
    encoded.textureSet.AddTexture(std::make_shared<Texture3D>(
      metadata.dimensions.x,
      metadata.dimensions.y,
      metadata.dimensions.z,
      GL_R8,
      GL_RED,
      GL_UNSIGNED_BYTE,
      volume.GetVoxels().data(),
      true
    ));
    return encoded;
  }
};

template <>
struct VolumeGpuEncoder<uint16_t>
{
  static EncodedVolumeResource Encode(const VolumeData<uint16_t>& volume)
  {
    EncodedVolumeResource encoded;
    const VolumeMetadata& metadata = volume.GetMetadata();
    encoded.dimensions = metadata.dimensions;
    encoded.spacing = metadata.spacing;
    encoded.voxelKind = metadata.voxelKind;
    encoded.textureSet.AddTexture(std::make_shared<Texture3D>(
      metadata.dimensions.x,
      metadata.dimensions.y,
      metadata.dimensions.z,
      GL_R16,
      GL_RED,
      GL_UNSIGNED_SHORT,
      volume.GetVoxels().data(),
      true
    ));
    return encoded;
  }
};

template <>
struct VolumeGpuEncoder<float>
{
  static EncodedVolumeResource Encode(const VolumeData<float>& volume)
  {
    EncodedVolumeResource encoded;
    const VolumeMetadata& metadata = volume.GetMetadata();
    encoded.dimensions = metadata.dimensions;
    encoded.spacing = metadata.spacing;
    encoded.voxelKind = metadata.voxelKind;
    encoded.textureSet.AddTexture(std::make_shared<Texture3D>(
      metadata.dimensions.x,
      metadata.dimensions.y,
      metadata.dimensions.z,
      GL_R32F,
      GL_RED,
      GL_FLOAT,
      volume.GetVoxels().data(),
      true
    ));
    return encoded;
  }
};

template <>
struct VolumeGpuEncoder<glm::mat3>
{
  static EncodedVolumeResource Encode(const VolumeData<glm::mat3>& volume)
  {
    EncodedVolumeResource encoded;
    const VolumeMetadata& metadata = volume.GetMetadata();
    encoded.dimensions = metadata.dimensions;
    encoded.spacing = metadata.spacing;
    encoded.voxelKind = metadata.voxelKind;
    encoded.matrixRows = 3;
    encoded.matrixCols = 3;

    const size_t voxelCount = volume.GetVoxelCount();
    std::vector<float> row0(voxelCount * 3);
    std::vector<float> row1(voxelCount * 3);
    std::vector<float> row2(voxelCount * 3);

    for (size_t i = 0; i < voxelCount; ++i)
    {
      const glm::mat3& matrix = volume.GetVoxels()[i];
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

    encoded.textureSet.AddTexture(std::make_shared<Texture3D>(metadata.dimensions.x, metadata.dimensions.y, metadata.dimensions.z, GL_RGB32F, GL_RGB, GL_FLOAT, row0.data(), true));
    encoded.textureSet.AddTexture(std::make_shared<Texture3D>(metadata.dimensions.x, metadata.dimensions.y, metadata.dimensions.z, GL_RGB32F, GL_RGB, GL_FLOAT, row1.data(), true));
    encoded.textureSet.AddTexture(std::make_shared<Texture3D>(metadata.dimensions.x, metadata.dimensions.y, metadata.dimensions.z, GL_RGB32F, GL_RGB, GL_FLOAT, row2.data(), true));
    return encoded;
  }
};
