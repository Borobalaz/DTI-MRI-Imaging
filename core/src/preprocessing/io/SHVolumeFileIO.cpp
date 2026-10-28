#include "Preprocessing/io/SHVolumeFileIO.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "scene/spherical_harmonics/RealSH.h"

namespace
{
  // Layout (native little-endian, written field by field so no struct padding reaches the file):
  //   char[4] "SHB1" | int32[3] dimensions | float32[3] spacing | int32 lmax | uint64 floatCount
  //   | float32[floatCount] coefficients (voxel-major, VolumeData::FlatIndex order)
  constexpr char kMagic[4] = {'S', 'H', 'B', '1'};
  constexpr std::uint64_t kHeaderSize = 40;

  template <typename T>
  void WriteValue(std::ofstream& output, const T& value)
  {
    output.write(reinterpret_cast<const char*>(&value), sizeof value);
  }

  template <typename T>
  void ReadValue(std::ifstream& input, T& value)
  {
    input.read(reinterpret_cast<char*>(&value), sizeof value);
  }

  [[noreturn]] void ThrowInvalid(const std::string& filePath, const std::string& reason)
  {
    throw std::runtime_error("Invalid SH volume file '" + filePath + "': " + reason);
  }
}

namespace SHVolumeFileIO
{
  /**
   * @brief Write the volume's header and dense coefficients to filePath (the mask is not written).
   */
  void Save(const std::string& filePath, const SHVolume& volume)
  {
    if (!volume.IsWellFormed())
    {
      throw std::runtime_error("Cannot save malformed SH volume to '" + filePath + "'.");
    }

    const std::filesystem::path path(filePath);
    if (!path.parent_path().empty())
    {
      std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
      throw std::runtime_error("Failed to open SH volume file '" + filePath + "' for writing.");
    }

    output.write(kMagic, sizeof kMagic);
    WriteValue(output, static_cast<std::int32_t>(volume.dimensions.x));
    WriteValue(output, static_cast<std::int32_t>(volume.dimensions.y));
    WriteValue(output, static_cast<std::int32_t>(volume.dimensions.z));
    WriteValue(output, static_cast<float>(volume.spacing.x));
    WriteValue(output, static_cast<float>(volume.spacing.y));
    WriteValue(output, static_cast<float>(volume.spacing.z));
    WriteValue(output, static_cast<std::int32_t>(volume.lmax));
    WriteValue(output, static_cast<std::uint64_t>(volume.coefficients.size()));
    output.write(reinterpret_cast<const char*>(volume.coefficients.data()),
                 static_cast<std::streamsize>(volume.coefficients.size() * sizeof(float)));

    output.flush();
    if (!output.good())
    {
      throw std::runtime_error("Failed to write SH volume file '" + filePath + "'.");
    }
  }

  /**
   * @brief Read and validate an SH volume file; the returned volume has no mask.
   */
  SHVolume Load(const std::string& filePath)
  {
    std::ifstream input(std::filesystem::path(filePath), std::ios::binary);
    if (!input)
    {
      ThrowInvalid(filePath, "cannot open file");
    }

    char magic[4] = {};
    std::int32_t dimensions[3] = {};
    float spacing[3] = {};
    std::int32_t lmax = 0;
    std::uint64_t floatCount = 0;
    input.read(magic, sizeof magic);
    for (std::int32_t& dimension : dimensions)
    {
      ReadValue(input, dimension);
    }
    for (float& component : spacing)
    {
      ReadValue(input, component);
    }
    ReadValue(input, lmax);
    ReadValue(input, floatCount);
    if (!input)
    {
      ThrowInvalid(filePath, "truncated header");
    }

    if (std::memcmp(magic, kMagic, sizeof kMagic) != 0)
    {
      ThrowInvalid(filePath, "bad magic (expected SHB1)");
    }
    if (dimensions[0] <= 0 || dimensions[1] <= 0 || dimensions[2] <= 0)
    {
      ThrowInvalid(filePath, "non-positive dimensions");
    }
    const int coefficientsPerVoxel = RealSH::CoefficientCount(lmax);
    if (coefficientsPerVoxel <= 0)
    {
      ThrowInvalid(filePath, "unsupported lmax " + std::to_string(lmax));
    }

    const std::uint64_t expected = static_cast<std::uint64_t>(dimensions[0]) * static_cast<std::uint64_t>(dimensions[1]) *
                                   static_cast<std::uint64_t>(dimensions[2]) * static_cast<std::uint64_t>(coefficientsPerVoxel);
    if (floatCount != expected)
    {
      ThrowInvalid(filePath, "coefficient count " + std::to_string(floatCount) + " does not match dimensions and lmax (" +
                                 std::to_string(expected) + ")");
    }
    if (expected > std::numeric_limits<size_t>::max() / sizeof(float))
    {
      ThrowInvalid(filePath, "volume is too large");
    }

    input.seekg(0, std::ios::end);
    const std::streamoff fileSize = input.tellg();
    if (fileSize < 0 || static_cast<std::uint64_t>(fileSize) != kHeaderSize + floatCount * sizeof(float))
    {
      ThrowInvalid(filePath, "file size does not match the header");
    }
    input.seekg(static_cast<std::streamoff>(kHeaderSize), std::ios::beg);

    SHVolume volume;
    volume.dimensions = glm::ivec3(dimensions[0], dimensions[1], dimensions[2]);
    volume.spacing = glm::vec3(spacing[0], spacing[1], spacing[2]);
    volume.lmax = lmax;
    volume.coefficients.resize(static_cast<size_t>(floatCount));
    input.read(reinterpret_cast<char*>(volume.coefficients.data()),
               static_cast<std::streamsize>(volume.coefficients.size() * sizeof(float)));
    if (!input)
    {
      ThrowInvalid(filePath, "truncated coefficient data");
    }

    for (const float coefficient : volume.coefficients)
    {
      if (!std::isfinite(coefficient))
      {
        ThrowInvalid(filePath, "non-finite coefficient");
      }
    }
    if (!volume.IsWellFormed())
    {
      ThrowInvalid(filePath, "volume is not well formed");
    }
    return volume;
  }
}
