#include "VolumeSeriesData.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

VolumeSeriesData::VolumeSeriesData(int width,
                                   int height,
                                   int depth,
                                   int frames,
                                   const glm::vec3 &spacing,
                                   float temporalSpacing)
    : dimensions(0, 0, 0),
      spacing(1.0f, 1.0f, 1.0f),
      frameCount(0),
      temporalSpacing(1.0f)
{
  Resize(width, height, depth, frames);
  this->spacing = spacing;
  this->temporalSpacing = temporalSpacing;
}

void VolumeSeriesData::Resize(int width, int height, int depth, int frames)
{
  if (width < 0 || height < 0 || depth < 0 || frames < 0)
  {
    throw std::invalid_argument("VolumeSeriesData dimensions cannot be negative.");
  }

  dimensions = glm::ivec3(width, height, depth);
  frameCount = frames;

  const size_t spatialVoxelCount =
      static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
  voxels.assign(spatialVoxelCount * static_cast<size_t>(frames), 0.0f);
}

VolumeData VolumeSeriesData::ExtractFrame(int frameIndex) const
{
  if (frameIndex < 0 || frameIndex >= frameCount)
  {
    throw std::out_of_range("VolumeSeriesData frame index is out of bounds.");
  }

  const int width = dimensions.x;
  const int height = dimensions.y;
  const int depth = dimensions.z;

  VolumeData frame(width, height, depth, spacing);
  std::vector<float> &frameVoxels = frame.GetVoxels();

  const size_t spatialVoxelCount =
      static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
  const size_t sourceOffset = spatialVoxelCount * static_cast<size_t>(frameIndex);

  std::copy(voxels.begin() + static_cast<std::ptrdiff_t>(sourceOffset),
            voxels.begin() + static_cast<std::ptrdiff_t>(sourceOffset + spatialVoxelCount),
            frameVoxels.begin());

  return frame;
}
