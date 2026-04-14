#pragma once

#include <cstdint>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>

struct VolumeMetadata
{
  glm::ivec3 dimensions{0, 0, 0};
  glm::vec3 spacing{1.0f, 1.0f, 1.0f};
};

class VolumeData
{
public:
  VolumeData();
  explicit VolumeData(const std::string &filePath);
  VolumeData(int width, int height, int depth, const glm::vec3 &spacing = glm::vec3(1.0f));

  static size_t FlatIndex(int x, int y, int z, int width, int height);
  
  void Resize(int width, int height, int depth);

  int GetWidth() const { return dimensions.x; }
  int GetHeight() const { return dimensions.y; }
  int GetDepth() const { return dimensions.z; }

  const glm::ivec3 &GetDimensions() const { return dimensions; }
  const glm::vec3 &GetSpacing() const { return spacing; }
  VolumeMetadata GetMetadata() const { return VolumeMetadata{dimensions, spacing}; }

  size_t GetVoxelCount() const { return voxels.size(); }
  std::vector<float> &GetVoxels() { return voxels; }
  const std::vector<float> &GetVoxels() const { return voxels; }

  float GetValue(glm::vec3 coord) const;

private:
  glm::ivec3 dimensions{0, 0, 0};
  glm::vec3 spacing{1.0f, 1.0f, 1.0f};
  std::vector<float> voxels;
};
