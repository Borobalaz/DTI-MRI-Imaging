#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>

#include "VolumeData.h"

class VolumeSeriesData
{
public:
  VolumeSeriesData() = default;

  VolumeSeriesData(int width,
                   int height,
                   int depth,
                   int frames,
                   const glm::vec3& spacing = glm::vec3(1.0f),
                   float temporalSpacing = 1.0f);

  void Resize(int width, int height, int depth, int frames);

  int GetWidth() const { return dimensions.x; }
  int GetHeight() const { return dimensions.y; }
  int GetDepth() const { return dimensions.z; }
  int GetFrameCount() const { return frameCount; }

  const glm::ivec3& GetDimensions() const { return dimensions; }
  const glm::vec3& GetSpacing() const { return spacing; }
  float GetTemporalSpacing() const { return temporalSpacing; }

  size_t GetVoxelCount() const { return voxels.size(); }
  std::vector<float>& GetVoxels() { return voxels; }
  const std::vector<float>& GetVoxels() const { return voxels; }

  VolumeData ExtractFrame(int frameIndex) const;

private:
  glm::ivec3 dimensions{0, 0, 0};
  glm::vec3 spacing{1.0f, 1.0f, 1.0f};
  int frameCount = 0;
  float temporalSpacing = 1.0f;
  std::vector<float> voxels;
};
