#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "SphericalHarmonics/RealSH.h"
#include "Volume/VolumeData.h"

// Dense per-voxel spherical-harmonic coefficients (RealSH convention) plus an optional scalar mask
// used to select which voxels get a glyph. CPU-only and immutable once shared, so it can be built
// on a preprocessing worker and read from a glyph-rebuild worker.
struct SHVolume
{
  glm::ivec3 dimensions{0};
  glm::vec3 spacing{1.0f};
  int lmax = RealSH::kSHMaxOrder;

  // Voxel-major in VolumeData::FlatIndex order, CoefficientsPerVoxel() floats per voxel.
  std::vector<float> coefficients;

  // Optional; when set it must have the same dimensions as the SH volume.
  std::shared_ptr<const VolumeData> mask;

  size_t CoefficientsPerVoxel() const
  {
    return static_cast<size_t>(RealSH::CoefficientCount(lmax));
  }

  size_t VoxelCount() const
  {
    return static_cast<size_t>(dimensions.x) * static_cast<size_t>(dimensions.y) * static_cast<size_t>(dimensions.z);
  }

  // Precondition: IsWellFormed() and (x, y, z) inside dimensions.
  const float* VoxelCoefficients(int x, int y, int z) const
  {
    return coefficients.data() + VolumeData::FlatIndex(x, y, z, dimensions.x, dimensions.y) * CoefficientsPerVoxel();
  }

  // Positive dimensions, a supported lmax, exactly VoxelCount() * CoefficientsPerVoxel()
  // coefficients, and a mask (if any) matching the dimensions.
  bool IsWellFormed() const
  {
    if (dimensions.x <= 0 || dimensions.y <= 0 || dimensions.z <= 0 || CoefficientsPerVoxel() == 0)
    {
      return false;
    }
    if (coefficients.size() != VoxelCount() * CoefficientsPerVoxel())
    {
      return false;
    }
    return !mask || mask->GetDimensions() == dimensions;
  }
};
