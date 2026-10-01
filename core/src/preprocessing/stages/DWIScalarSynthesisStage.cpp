#include "Preprocessing/stages/DWIScalarSynthesisStage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Preprocessing/MriPreprocessingStages.h"

namespace
{
  struct SymmetricTensor3x3
  {
    float xx;
    float yy;
    float zz;
    float xy;
    float xz;
    float yz;
  };

  float SafeSqrt(float value)
  {
    return std::sqrt(std::max(value, 0.0f));
  }

  // Jacobi diagonalization for symmetric 3x3 tensors.
  std::array<float, 3> EigenvaluesSymmetric(const SymmetricTensor3x3 &t)
  {
    float a[3][3] = {
      {t.xx, t.xy, t.xz},
      {t.xy, t.yy, t.yz},
      {t.xz, t.yz, t.zz}};

    for (int iter = 0; iter < 50; ++iter)
    {
      int p = 0;
      int q = 1;
      float maxAbs = std::fabs(a[0][1]);

      const float a02 = std::fabs(a[0][2]);
      if (a02 > maxAbs)
      {
        maxAbs = a02;
        p = 0;
        q = 2;
      }

      const float a12 = std::fabs(a[1][2]);
      if (a12 > maxAbs)
      {
        maxAbs = a12;
        p = 1;
        q = 2;
      }

      if (maxAbs < 1e-10f)
      {
        break;
      }

      const float app = a[p][p];
      const float aqq = a[q][q];
      const float apq = a[p][q];

      const float tau = (aqq - app) / (2.0f * apq);
      const float tval = (tau >= 0.0f)
                           ? 1.0f / (tau + SafeSqrt(1.0f + tau * tau))
                           : -1.0f / (-tau + SafeSqrt(1.0f + tau * tau));
      const float c = 1.0f / SafeSqrt(1.0f + tval * tval);
      const float s = tval * c;

      for (int k = 0; k < 3; ++k)
      {
        if (k == p || k == q)
        {
          continue;
        }

        const float akp = a[k][p];
        const float akq = a[k][q];
        a[k][p] = c * akp - s * akq;
        a[p][k] = a[k][p];
        a[k][q] = s * akp + c * akq;
        a[q][k] = a[k][q];
      }

      const float newApp = c * c * app - 2.0f * s * c * apq + s * s * aqq;
      const float newAqq = s * s * app + 2.0f * s * c * apq + c * c * aqq;
      a[p][p] = newApp;
      a[q][q] = newAqq;
      a[p][q] = 0.0f;
      a[q][p] = 0.0f;
    }

    std::array<float, 3> eigen = {a[0][0], a[1][1], a[2][2]};
    std::sort(eigen.begin(), eigen.end(), std::greater<float>());
    return eigen;
  }
}

/**
 * @brief Get the name of the preprocessing stage.
 *
 * @return const char*
 */
const char *DWIScalarSynthesisStage::Name() const
{
  return "DWI scalar synthesis";
}

/**
 * @brief Build FA, MD, AD, RD, and eigenvalue volumes from the DTI tensor channels in the context.
 * 
 * @param context 
 */
void DWIScalarSynthesisStage::Execute(MriPreprocessingContext &context) const
{
  const VolumeMetadata &metadata = context.outputChannels.Dxx.GetMetadata();
  const int width = metadata.dimensions.x;
  const int height = metadata.dimensions.y;
  const int depth = metadata.dimensions.z;

  VolumeData fa(width, height, depth, metadata.spacing);
  VolumeData md(width, height, depth, metadata.spacing);
  VolumeData ad(width, height, depth, metadata.spacing);
  VolumeData rd(width, height, depth, metadata.spacing);
  VolumeData L1(width, height, depth, metadata.spacing);
  VolumeData L2(width, height, depth, metadata.spacing);
  VolumeData L3(width, height, depth, metadata.spacing);

  // Get the tensor
  const std::vector<float> &dxx = context.outputChannels.Dxx.GetVoxels();
  const std::vector<float> &dyy = context.outputChannels.Dyy.GetVoxels();
  const std::vector<float> &dzz = context.outputChannels.Dzz.GetVoxels();
  const std::vector<float> &dxy = context.outputChannels.Dxy.GetVoxels();
  const std::vector<float> &dxz = context.outputChannels.Dxz.GetVoxels();
  const std::vector<float> &dyz = context.outputChannels.Dyz.GetVoxels();

  // Output arrays
  std::vector<float> &faOut = fa.GetVoxels();
  std::vector<float> &mdOut = md.GetVoxels();
  std::vector<float> &adOut = ad.GetVoxels();
  std::vector<float> &rdOut = rd.GetVoxels();
  std::vector<float> &l1Out = L1.GetVoxels();
  std::vector<float> &l2Out = L2.GetVoxels();
  std::vector<float> &l3Out = L3.GetVoxels();


  constexpr float eps = 1e-12f;

  const size_t voxelCount = dxx.size();
  for (size_t i = 0; i < voxelCount; ++i)
  {
    // Null bad voxels (NaN, inf, ...)
    if (!std::isfinite(dxx[i]) || !std::isfinite(dyy[i]) || !std::isfinite(dzz[i]) ||
        !std::isfinite(dxy[i]) || !std::isfinite(dxz[i]) || !std::isfinite(dyz[i]))
    {
      faOut[i] = 0.0f;
      mdOut[i] = 0.0f;
      adOut[i] = 0.0f;
      rdOut[i] = 0.0f;
      l1Out[i] = 0.0f;
      l2Out[i] = 0.0f;  
      l3Out[i] = 0.0f;
      continue;
    }

    const SymmetricTensor3x3 tensor{
      dxx[i], dyy[i], dzz[i],
      dxy[i], dxz[i], dyz[i]};

    // Eigenvalues
    const std::array<float, 3> eigen = EigenvaluesSymmetric(tensor);
    const float lambda1 = eigen[0];
    const float lambda2 = eigen[1];
    const float lambda3 = eigen[2];

    // MD, RD, FA, AD
    const float meanDiffusivity = (lambda1 + lambda2 + lambda3) / 3.0f;
    const float radialDiffusivity = (lambda2 + lambda3) / 2.0f;

    const float num = (lambda1 - meanDiffusivity) * (lambda1 - meanDiffusivity) +
                      (lambda2 - meanDiffusivity) * (lambda2 - meanDiffusivity) +
                      (lambda3 - meanDiffusivity) * (lambda3 - meanDiffusivity);
    const float den = lambda1 * lambda1 + lambda2 * lambda2 + lambda3 * lambda3;
    const float fractionalAnisotropy = (den > eps)
      ? SafeSqrt(1.5f * num / den)
      : 0.0f;

    // Set outputs
    faOut[i] = std::clamp(fractionalAnisotropy, 0.0f, 1.0f);
    mdOut[i] = meanDiffusivity;
    adOut[i] = lambda1;
    rdOut[i] = radialDiffusivity;
    l1Out[i] = lambda1;
    l2Out[i] = lambda2;
    l3Out[i] = lambda3;
  }

  // Move outputs to context
  context.outputChannels.FA = std::move(fa);
  context.outputChannels.MD = std::move(md);
  context.outputChannels.AD = std::move(ad);
  context.outputChannels.RD = std::move(rd);
  context.outputChannels.L1 = std::move(L1);
  context.outputChannels.L2 = std::move(L2);
  context.outputChannels.L3 = std::move(L3);
}

/**
 * @brief Create a Dwi Scalar Synthesis Stage object
 * 
 * @return std::unique_ptr<IMriPreprocessingStage> 
 */
std::unique_ptr<IMriPreprocessingStage> CreateDwiScalarSynthesisStage()
{
  return std::make_unique<DWIScalarSynthesisStage>();
}
