#include "VolumeData.h"

#include <utility>

#include "VolumeFileLoader.h"

namespace
{
  VolumeData LoadOrThrowVolumeData(const std::string &filePath)
  {
    const std::optional<VolumeData> loaded = VolumeFileLoader::LoadTyped(filePath);
    if (!loaded.has_value())
    {
      throw std::invalid_argument("Failed to load volume data from file: " + filePath);
    }

    return *loaded;
  }
}

/**
 * @brief Construct a new Volume Data:: Volume Data object
 * 
 */
VolumeData::VolumeData()
    : metadata(), voxels()
{
}

/**
 * @brief Construct a new Volume Data:: Volume Data object
 * 
 * @param filePath 
 */
VolumeData::VolumeData(const std::string &filePath)
    : VolumeData(LoadOrThrowVolumeData(filePath))
{
}

/**
 * @brief Construct a new Volume Data:: Volume Data object
 * 
 * @param width 
 * @param height 
 * @param depth 
 * @param spacing 
 */
VolumeData::VolumeData(int width, int height, int depth, const glm::vec3 &spacing): VolumeData()
{
  Resize(width, height, depth);
  metadata.spacing = spacing;
}

/**
 * @brief Change the dimensions of the volume and resize the voxel array accordingly. Existing voxel data will be discarded.
 * 
 * @param width 
 * @param height 
 * @param depth 
 */
void VolumeData::Resize(int width, int height, int depth)
{
  if (width <= 0 || height <= 0 || depth <= 0)
  {
    throw std::invalid_argument("Volume dimensions must be positive.");
  }

  metadata.dimensions = glm::ivec3(width, height, depth);
  voxels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth));
}

/**
 * @brief Get the flat voxel array index corresponding to the given 3D coordinate. Throws if out of bounds.
 * 
 * @param x 
 * @param y 
 * @param z 
 * @return size_t 
 */
size_t VolumeData::GetFlatIndex(int x, int y, int z) const
{
  if (x < 0 || y < 0 || z < 0 ||
      x >= metadata.dimensions.x ||
      y >= metadata.dimensions.y ||
      z >= metadata.dimensions.z)
  {
    throw std::out_of_range("VolumeData index is out of bounds.");
  }

  return static_cast<size_t>(z) * 
    static_cast<size_t>(metadata.dimensions.y) * 
    static_cast<size_t>(metadata.dimensions.x) + static_cast<size_t>(y) * 
    static_cast<size_t>(metadata.dimensions.x) + static_cast<size_t>(x);
}

/**
 * @brief Get the voxel value at the given 3D coordinate. Currently uses nearest neighbor lookup; interpolation may be added in the future.
 * 
 * @param x 
 * @param y 
 * @param z 
 * @return float 
 */
float VolumeData::GetValue(glm::vec3 coord) const
{
  // TODO add interpolation
  int x = static_cast<int>(coord.x);
  int y = static_cast<int>(coord.y);
  int z = static_cast<int>(coord.z);
  size_t index = GetFlatIndex(x, y, z);
  return voxels[index];
}