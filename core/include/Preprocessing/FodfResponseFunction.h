#pragma once

#include <cstddef>
#include <vector>

// Axially symmetric single-fibre response of CSD (Tournier 2007), from a joint SH fit over the
// selected response voxels (each voxel's gradients rotated so its principal eigenvector is +z).
// Index k of both vectors <-> l = 2k, so each holds lmax/2 + 1 entries.
// zonal = R_l0 (m=0 SH coefficient of the response), rotational = r_l = R_l0 / sqrt((2l+1)/(4*pi)).
struct FodfResponseFunction
{
  float bValue = 0.0f;
  int lmax = 0;
  std::vector<float> zonalCoefficients;
  std::vector<float> rotationalCoefficients;
  size_t voxelCount = 0;
  float meanFa = 0.0f;

  bool IsValid() const
  {
    return lmax >= 0 && lmax % 2 == 0 &&
           rotationalCoefficients.size() == static_cast<size_t>(lmax / 2 + 1) &&
           zonalCoefficients.size() == rotationalCoefficients.size() &&
           rotationalCoefficients[0] > 0.0f;
  }
};
