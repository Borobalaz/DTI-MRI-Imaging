#pragma once

#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>

enum class VolumeVoxelKind : uint32_t
{
  UInt8 = 1,
  UInt16 = 2,
  Float32 = 3,
  Mat3Float32 = 4
};

template <typename TVoxel>
struct VolumeVoxelTraits;

template <>
struct VolumeVoxelTraits<uint8_t>
{
  static constexpr VolumeVoxelKind Kind = VolumeVoxelKind::UInt8;
  static constexpr uint32_t ComponentCount = 1;
  static constexpr uint32_t MatrixRows = 1;
  static constexpr uint32_t MatrixCols = 1;
};

template <>
struct VolumeVoxelTraits<uint16_t>
{
  static constexpr VolumeVoxelKind Kind = VolumeVoxelKind::UInt16;
  static constexpr uint32_t ComponentCount = 1;
  static constexpr uint32_t MatrixRows = 1;
  static constexpr uint32_t MatrixCols = 1;
};

template <>
struct VolumeVoxelTraits<float>
{
  static constexpr VolumeVoxelKind Kind = VolumeVoxelKind::Float32;
  static constexpr uint32_t ComponentCount = 1;
  static constexpr uint32_t MatrixRows = 1;
  static constexpr uint32_t MatrixCols = 1;
};

template <>
struct VolumeVoxelTraits<glm::mat3>
{
  static constexpr VolumeVoxelKind Kind = VolumeVoxelKind::Mat3Float32;
  static constexpr uint32_t ComponentCount = 9;
  static constexpr uint32_t MatrixRows = 3;
  static constexpr uint32_t MatrixCols = 3;
};

struct VolumeMetadata
{
  glm::ivec3 dimensions{0, 0, 0};
  glm::vec3 spacing{1.0f, 1.0f, 1.0f};
  VolumeVoxelKind voxelKind{VolumeVoxelKind::Float32};
  uint32_t componentCount{1};
  uint32_t matrixRows{1};
  uint32_t matrixCols{1};
};

template <typename TVoxel>
class VolumeData
{
public:
  VolumeData()
  {
    static_assert(std::is_trivially_copyable_v<TVoxel>, "Volume voxels must be trivially copyable.");
    metadata.voxelKind = VolumeVoxelTraits<TVoxel>::Kind;
    metadata.componentCount = VolumeVoxelTraits<TVoxel>::ComponentCount;
    metadata.matrixRows = VolumeVoxelTraits<TVoxel>::MatrixRows;
    metadata.matrixCols = VolumeVoxelTraits<TVoxel>::MatrixCols;
  }

  VolumeData(int width, int height, int depth, const glm::vec3& spacing = glm::vec3(1.0f))
    : VolumeData()
  {
    Resize(width, height, depth);
    metadata.spacing = spacing;
  }

  void Resize(int width, int height, int depth)
  {
    if (width <= 0 || height <= 0 || depth <= 0)
    {
      throw std::invalid_argument("Volume dimensions must be positive.");
    }

    metadata.dimensions = glm::ivec3(width, height, depth);
    voxels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth));
  }

  int GetWidth() const { return metadata.dimensions.x; }
  int GetHeight() const { return metadata.dimensions.y; }
  int GetDepth() const { return metadata.dimensions.z; }
  size_t GetVoxelCount() const { return voxels.size(); }

  const VolumeMetadata& GetMetadata() const { return metadata; }
  void SetSpacing(const glm::vec3& spacing) { metadata.spacing = spacing; }

  TVoxel& At(int x, int y, int z)
  {
    return voxels.at(GetFlatIndex(x, y, z));
  }

  const TVoxel& At(int x, int y, int z) const
  {
    return voxels.at(GetFlatIndex(x, y, z));
  }

  std::vector<TVoxel>& GetVoxels() { return voxels; }
  const std::vector<TVoxel>& GetVoxels() const { return voxels; }

private:
  size_t GetFlatIndex(int x, int y, int z) const
  {
    if (x < 0 || y < 0 || z < 0 ||
        x >= metadata.dimensions.x ||
        y >= metadata.dimensions.y ||
        z >= metadata.dimensions.z)
    {
      throw std::out_of_range("VolumeData index is out of bounds.");
    }

    return static_cast<size_t>(z) * static_cast<size_t>(metadata.dimensions.y) * static_cast<size_t>(metadata.dimensions.x)
      + static_cast<size_t>(y) * static_cast<size_t>(metadata.dimensions.x)
      + static_cast<size_t>(x);
  }

  VolumeMetadata metadata;
  std::vector<TVoxel> voxels;
};
