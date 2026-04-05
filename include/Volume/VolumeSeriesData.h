#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>

#include "VolumeData.h"

struct VolumeSeriesMetadata
{
  glm::ivec3 dimensions{0, 0, 0};
  glm::vec3 spacing{1.0f, 1.0f, 1.0f};
  int frameCount = 0;
  float temporalSpacing = 1.0f;
};

class VolumeSeriesData
{
public:
  VolumeSeriesData() = default;

  VolumeSeriesData(int width,
                   int height,
                   int depth,
                   int frames,
                   const glm::vec3& spacing = glm::vec3(1.0f),
                   float temporalSpacing = 1.0f)
    : metadata{}
  {
    Resize(width, height, depth, frames);
    metadata.spacing = spacing;
    metadata.temporalSpacing = temporalSpacing;
  }

  void Resize(int width, int height, int depth, int frames)
  {
    if (width < 0 || height < 0 || depth < 0 || frames < 0)
    {
      throw std::invalid_argument("VolumeSeriesData dimensions cannot be negative.");
    }

    metadata.dimensions = glm::ivec3(width, height, depth);
    metadata.frameCount = frames;

    const size_t spatialVoxelCount =
      static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
    voxels.assign(spatialVoxelCount * static_cast<size_t>(frames), 0.0f);
  }

  int GetWidth() const { return metadata.dimensions.x; }
  int GetHeight() const { return metadata.dimensions.y; }
  int GetDepth() const { return metadata.dimensions.z; }
  int GetFrameCount() const { return metadata.frameCount; }

  const VolumeSeriesMetadata& GetMetadata() const { return metadata; }

  size_t GetVoxelCount() const { return voxels.size(); }
  std::vector<float>& GetVoxels() { return voxels; }
  const std::vector<float>& GetVoxels() const { return voxels; }

  VolumeData ExtractFrame(int frameIndex) const
  {
    if (frameIndex < 0 || frameIndex >= metadata.frameCount)
    {
      throw std::out_of_range("VolumeSeriesData frame index is out of bounds.");
    }

    const int width = metadata.dimensions.x;
    const int height = metadata.dimensions.y;
    const int depth = metadata.dimensions.z;

    VolumeData frame(width, height, depth, metadata.spacing);
    std::vector<float>& frameVoxels = frame.GetVoxels();

    const size_t spatialVoxelCount =
      static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
    const size_t sourceOffset = spatialVoxelCount * static_cast<size_t>(frameIndex);

    std::copy(voxels.begin() + static_cast<std::ptrdiff_t>(sourceOffset),
              voxels.begin() + static_cast<std::ptrdiff_t>(sourceOffset + spatialVoxelCount),
              frameVoxels.begin());

    return frame;
  }

private:
  VolumeSeriesMetadata metadata;
  std::vector<float> voxels;
};
