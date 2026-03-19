#pragma once

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <variant>

#include <glm/glm.hpp>

#include "VolumeData.h"

struct VolumeFileHeader
{
  char magic[4] = {'V', 'X', 'A', '1'};
  uint32_t version = 1;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 0;
  float spacingX = 1.0f;
  float spacingY = 1.0f;
  float spacingZ = 1.0f;
};

using LoadedVolumeVariant = std::variant<
  VolumeData<uint8_t>,
  VolumeData<uint16_t>,
  VolumeData<float>,
  VolumeData<glm::mat3>>;

class VolumeFileLoader
{
public:
  static std::optional<LoadedVolumeVariant> Load(const std::string& filePath);

  template <typename TVoxel>
  static std::optional<VolumeData<TVoxel>> LoadTyped(const std::string& filePath)
  {
    if (const auto vxa = LoadTypedVxa<TVoxel>(filePath))
    {
      return vxa;
    }

    return TryLoadMedicalFormat<TVoxel>(filePath);
  }

  template <typename TVoxel>
  static bool Save(const std::string& filePath, const VolumeData<TVoxel>& volume)
  {
    const std::filesystem::path path(filePath);
    if (path.has_parent_path())
    {
      std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output(filePath, std::ios::binary);
    if (!output.is_open())
    {
      return false;
    }

    const VolumeMetadata& metadata = volume.GetMetadata();
    VolumeFileHeader header{};
    header.width = static_cast<uint32_t>(metadata.dimensions.x);
    header.height = static_cast<uint32_t>(metadata.dimensions.y);
    header.depth = static_cast<uint32_t>(metadata.dimensions.z);
    header.spacingX = metadata.spacing.x;
    header.spacingY = metadata.spacing.y;
    header.spacingZ = metadata.spacing.z;

    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    const std::vector<TVoxel>& voxels = volume.GetVoxels();
    output.write(reinterpret_cast<const char*>(voxels.data()), static_cast<std::streamsize>(voxels.size() * sizeof(TVoxel)));
    return output.good();
  }

private:
  template <typename TVoxel>
  static std::optional<VolumeData<TVoxel>> LoadTypedVxa(const std::string& filePath)
  {
    std::ifstream input(filePath, std::ios::binary);
    if (!input.is_open())
    {
      return std::nullopt;
    }

    VolumeFileHeader header{};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!input.good())
    {
      return std::nullopt;
    }

    if (std::memcmp(header.magic, "VXA1", 4) != 0 ||
        header.version != 1)
    {
      return std::nullopt;
    }

    if (header.width == 0 || header.height == 0 || header.depth == 0)
    {
      return std::nullopt;
    }

    const uint64_t voxelCount =
      static_cast<uint64_t>(header.width) *
      static_cast<uint64_t>(header.height) *
      static_cast<uint64_t>(header.depth);
    if (voxelCount > static_cast<uint64_t>(std::numeric_limits<size_t>::max() / sizeof(TVoxel)))
    {
      return std::nullopt;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff fileSize = input.tellg();
    if (fileSize < 0)
    {
      return std::nullopt;
    }

    const uint64_t expectedSize = sizeof(VolumeFileHeader) + voxelCount * sizeof(TVoxel);
    if (static_cast<uint64_t>(fileSize) != expectedSize)
    {
      return std::nullopt;
    }

    input.seekg(static_cast<std::streamoff>(sizeof(VolumeFileHeader)), std::ios::beg);

    VolumeData<TVoxel> volume(
      static_cast<int>(header.width),
      static_cast<int>(header.height),
      static_cast<int>(header.depth),
      glm::vec3(header.spacingX, header.spacingY, header.spacingZ)
    );

    std::vector<TVoxel>& voxels = volume.GetVoxels();
    input.read(reinterpret_cast<char*>(voxels.data()), static_cast<std::streamsize>(voxels.size() * sizeof(TVoxel)));
    if (!input.good())
    {
      return std::nullopt;
    }

    return volume;
  }

  template <typename TVoxel>
  static std::optional<VolumeData<TVoxel>> TryLoadMedicalFormat(const std::string&)
  {
    return std::nullopt;
  }
};

template <>
std::optional<VolumeData<uint8_t>> VolumeFileLoader::TryLoadMedicalFormat<uint8_t>(
  const std::string& filePath);

template <>
std::optional<VolumeData<uint16_t>> VolumeFileLoader::TryLoadMedicalFormat<uint16_t>(
  const std::string& filePath);

template <>
std::optional<VolumeData<float>> VolumeFileLoader::TryLoadMedicalFormat<float>(
  const std::string& filePath);
