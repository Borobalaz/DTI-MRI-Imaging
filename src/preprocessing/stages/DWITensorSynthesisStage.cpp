#include "Preprocessing/stages/DWITensorSynthesisStage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

#include "Preprocessing/MriPreprocessingStages.h"
#include "VolumeFileLoader.h"

namespace
{
  inline size_t FlatIndex(int x, int y, int z, int width, int height)
  {
    return static_cast<size_t>(z) * static_cast<size_t>(height) * static_cast<size_t>(width) +
           static_cast<size_t>(y) * static_cast<size_t>(width) +
           static_cast<size_t>(x);
  }

  inline float ClampPositiveSignal(float value)
  {
    return std::max(value, 1e-6f);
  }

}

/**
 * @brief Get the name of the preprocessing stage.
 *
 * @return const char*
 */
const char *DWITensorSynthesisStage::Name() const
{
  return "DWI tensor OLS fit";
}

/**
 * @brief Fit DTI tensor coefficients and store them in the context's outputChannels.
 *
 * @param context
 */
void DWITensorSynthesisStage::Execute(MriPreprocessingContext &context) const
{
  if (!context.gradientMetadataValid)
  {
    throw std::runtime_error("DWI gradient metadata must be validated before tensor fitting.");
  }

  // Load the raw MRI data
  const auto dwiSeries = VolumeFileLoader::LoadSeries(context.selectedDwiVolumePath);
  if (!dwiSeries.has_value())
  {
    throw std::runtime_error(
        "Failed to load DWI series from '" + context.selectedDwiVolumePath +
        "'. loaderError='" + VolumeFileLoader::GetLastError() + "'.");
  }

  // comment
  const int frameCount = dwiSeries->GetFrameCount();
  if (frameCount <= 0)
  {
    throw std::runtime_error("DWI series contains zero frames.");
  }
  std::cout << "DWI series loaded with " << frameCount << " frames." << std::endl;

  if (static_cast<size_t>(frameCount) != context.bValues.size() ||
      static_cast<size_t>(frameCount) != context.gradientDirections.size())
  {
    std::ostringstream details;
    details
        << "DWI frame/gradient mismatch: frames=" << frameCount
        << " bvals=" << context.bValues.size()
        << " bvecs=" << context.gradientDirections.size()
        << " path='" << context.selectedDwiVolumePath << "'.";
    throw std::runtime_error(details.str());
  }

  const VolumeSeriesMetadata &metadata = dwiSeries->GetMetadata();
  const int width = metadata.dimensions.x;
  const int height = metadata.dimensions.y;
  const int depth = metadata.dimensions.z;

  if (width <= 0 || height <= 0 || depth <= 0)
  {
    throw std::runtime_error("DWI spatial dimensions are invalid; cannot synthesize DTI tensor channels.");
  }

  const size_t spatialVoxelCount =
      static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
  const std::vector<float> &allSignals = dwiSeries->GetVoxels();

  if (allSignals.size() != spatialVoxelCount * static_cast<size_t>(frameCount))
  {
    throw std::runtime_error("DWI series voxel count does not match metadata dimensions.");
  }

  // empty output channels
  VolumeData Dxx(width, height, depth, metadata.spacing);
  VolumeData Dyy(width, height, depth, metadata.spacing);
  VolumeData Dzz(width, height, depth, metadata.spacing);
  VolumeData Dxy(width, height, depth, metadata.spacing);
  VolumeData Dxz(width, height, depth, metadata.spacing);
  VolumeData Dyz(width, height, depth, metadata.spacing);

  std::vector<float> &DxxVoxels = Dxx.GetVoxels();
  std::vector<float> &DyyVoxels = Dyy.GetVoxels();
  std::vector<float> &DzzVoxels = Dzz.GetVoxels();
  std::vector<float> &DxyVoxels = Dxy.GetVoxels();
  std::vector<float> &DxzVoxels = Dxz.GetVoxels();
  std::vector<float> &DyzVoxels = Dyz.GetVoxels();

  std::vector<float> voxelSignal(static_cast<size_t>(frameCount), 0.0f);

  // OLS fit a tensor to each voxel's signal profile across the diffusion-weighted volumes
  for (int z = 0; z < depth; ++z)
  {
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        const size_t index = FlatIndex(x, y, z, width, height);
        for (int frame = 0; frame < frameCount; ++frame)
        {
          const size_t frameOffset = static_cast<size_t>(frame) * spatialVoxelCount;
          voxelSignal[static_cast<size_t>(frame)] = allSignals[frameOffset + index];
        }

        const std::array<float, 6> d =
            EstimateTensorFromSignal(voxelSignal, context.bValues, context.gradientDirections);

        const float dxx = d[0];
        const float dyy = d[1];
        const float dzz = d[2];
        const float dxy = d[3];
        const float dxz = d[4];
        const float dyz = d[5];

        DxxVoxels[index] = dxx;
        DyyVoxels[index] = dyy;
        DzzVoxels[index] = dzz;
        DxyVoxels[index] = dxy;
        DxzVoxels[index] = dxz;
        DyzVoxels[index] = dyz;
      }
    }
  }

  context.outputChannels.Dxx = std::move(Dxx);
  context.outputChannels.Dyy = std::move(Dyy);
  context.outputChannels.Dzz = std::move(Dzz);
  context.outputChannels.Dxy = std::move(Dxy);
  context.outputChannels.Dxz = std::move(Dxz);
  context.outputChannels.Dyz = std::move(Dyz);
}

/**
 * @brief OLS fit a diffusion tensor to a voxel's DWI signal profile across the diffusion-weighted volumes, given the gradient metadata.
 *
 * @param dwiSignal
 * @param bValues
 * @param gradients
 * @return std::array<float, 6>
 */
std::array<float, 6> DWITensorSynthesisStage::EstimateTensorFromSignal(
    const std::vector<float> &dwiSignal,
    const std::vector<float> &bValues,
    const std::vector<glm::vec3> &gradients) const
{
  if (dwiSignal.size() != bValues.size() || dwiSignal.size() != gradients.size())
  {
    throw std::runtime_error("Signal/gradient metadata size mismatch during tensor estimation.");
  }

  // Estimate S0 as the mean of the b0 signals, with a floor to prevent log-domain issues.
  float s0Sum = 0.0f;
  int s0Count = 0;
  for (size_t i = 0; i < bValues.size(); ++i)
  {
    if (bValues[i] <= 50.0f)
    {
      s0Sum += ClampPositiveSignal(dwiSignal[i]);
      ++s0Count;
    }
  }

  if (s0Count == 0)
  {
    throw std::runtime_error("No b0 volumes were found (b <= 50) for DTI reconstruction.");
  }

  const float s0 = std::max(s0Sum / static_cast<float>(s0Count), 1e-6f);

  // Set up the normal equations for OLS fitting of the log-signal to the design matrix rows.
  std::array<std::array<float, 6>, 6> ata{};
  std::array<float, 6> atb{};
  int diffusionEquationCount = 0;

  // Loop over diffusion-weighted volumes only (b > 50), 
  //  since b0 volumes are mainly dominated by T2 effects and would bias the fit if included.
  for (size_t i = 0; i < bValues.size(); ++i)
  {
    const float b = bValues[i];
    if (b <= 50.0f)
    {
      continue;
    }

    // This row is based on the Stejskal-Tanner equation: ln(S) = ln(S0) - b * g^T * D * g, where D is the 3x3 diffusion tensor.
    const std::array<float, 6> row = BuildDesignRow(b, gradients[i]);
    // The target value is the log of the normalized signal: ln(S / S0) = -b * g^T * D * g.
    const float y = std::log(ClampPositiveSignal(dwiSignal[i]) / s0);

    // Accumulate the normal equations: A^T * A and A^T * y.
    for (int r = 0; r < 6; ++r)
    {
      atb[r] += row[r] * y;
      for (int c = 0; c < 6; ++c)
      {
        ata[r][c] += row[r] * row[c];
      }
    }

    ++diffusionEquationCount;
  }

  if (diffusionEquationCount < 6)
  {
    throw std::runtime_error("At least 6 diffusion-weighted directions are required for tensor fitting.");
  }

  // Small Tikhonov stabilization for noisy/near-degenerate direction sets.
  for (int i = 0; i < 6; ++i)
  {
    ata[i][i] += 1e-6f;
  }

  // Solve the normal equations to get the tensor coefficients.
  return SolveSymmetricSystem6x6(ata, atb);
}

/**
 * @brief Build a design row for the DTI tensor fitting system.
 *
 * @param b The b-value.
 * @param g The gradient direction.
 * @return std::array<float, 6> The design row.
 */
std::array<float, 6> DWITensorSynthesisStage::BuildDesignRow(float b, const glm::vec3 &g)
{
  return {
      -b * g.x * g.x,
      -b * g.y * g.y,
      -b * g.z * g.z,
      -2.0f * b * g.x * g.y,
      -2.0f * b * g.x * g.z,
      -2.0f * b * g.y * g.z};
}

/**
 * @brief Solve a symmetric 6x6 linear system using Gaussian elimination with partial pivoting.
 *
 * @param normalMatrix The symmetric normal matrix.
 * @param rhs The right-hand side vector.
 * @return std::array<float, 6> The solution vector.
 */
std::array<float, 6> DWITensorSynthesisStage::SolveSymmetricSystem6x6(
    std::array<std::array<float, 6>, 6> normalMatrix,
    std::array<float, 6> rhs)
{
  for (int pivot = 0; pivot < 6; ++pivot)
  {
    int bestRow = pivot;
    float bestAbs = std::fabs(normalMatrix[pivot][pivot]);
    for (int row = pivot + 1; row < 6; ++row)
    {
      const float candidate = std::fabs(normalMatrix[row][pivot]);
      if (candidate > bestAbs)
      {
        bestAbs = candidate;
        bestRow = row;
      }
    }

    if (bestAbs < 1e-8f)
    {
      throw std::runtime_error("Ill-conditioned DTI normal matrix (insufficient directional diversity).");
    }

    if (bestRow != pivot)
    {
      std::swap(normalMatrix[pivot], normalMatrix[bestRow]);
      std::swap(rhs[pivot], rhs[bestRow]);
    }

    const float pivotValue = normalMatrix[pivot][pivot];
    for (int col = pivot; col < 6; ++col)
    {
      normalMatrix[pivot][col] /= pivotValue;
    }
    rhs[pivot] /= pivotValue;

    for (int row = 0; row < 6; ++row)
    {
      if (row == pivot)
      {
        continue;
      }

      const float factor = normalMatrix[row][pivot];
      if (std::fabs(factor) < 1e-12f)
      {
        continue;
      }

      for (int col = pivot; col < 6; ++col)
      {
        normalMatrix[row][col] -= factor * normalMatrix[pivot][col];
      }
      rhs[row] -= factor * rhs[pivot];
    }
  }

  return rhs;
}

/**
 * @brief Create a DWI tensor fitting stage object.
 *
 * @return std::unique_ptr<IMriPreprocessingStage>
 */
std::unique_ptr<IMriPreprocessingStage> CreateDwiTensorSynthesisStage()
{
  return std::make_unique<DWITensorSynthesisStage>();
}
