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
  
  void Resize(int width, int height, int depth);

  int GetWidth() const { return metadata.dimensions.x; }
  int GetHeight() const { return metadata.dimensions.y; }
  int GetDepth() const { return metadata.dimensions.z; }

  const VolumeMetadata &GetMetadata() const { return metadata; }

  size_t GetVoxelCount() const { return voxels.size(); }
  std::vector<float> &GetVoxels() { return voxels; }
  const std::vector<float> &GetVoxels() const { return voxels; }

  float GetValue(glm::vec3 coord) const;

private:
  size_t GetFlatIndex(int x, int y, int z) const;
  
  VolumeMetadata metadata;
  std::vector<float> voxels;
};
