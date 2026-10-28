#include "Preprocessing/stages/dti/DtiTensorShStage.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "scene/geometry/IcosphereGeometry.h"
#include "scene/glyphs/SHVolume.h"
#include "Preprocessing/stages/dti/DtiPreprocessingStages.h"
#include "scene/spherical_harmonics/RealSH.h"

namespace
{
  constexpr unsigned int kFitDirectionLevel = 3; // 642 directions
  constexpr double kFitRegularization = 1e-6;
  constexpr float kMinFractionalAnisotropy = 1e-3f;

  // Inverse of the symmetric tensor, or nullopt when it isn't positive definite (Sylvester's
  // criterion on the leading principal minors).
  std::optional<glm::dmat3> InvertPositiveDefinite(double xx, double yy, double zz, double xy, double xz, double yz)
  {
    const double minor2 = xx * yy - xy * xy;
    const double determinant = xx * (yy * zz - yz * yz) - xy * (xy * zz - yz * xz) + xz * (xy * yz - yy * xz);
    if (!(xx > 0.0) || !(minor2 > 0.0) || !(determinant > 0.0))
    {
      return std::nullopt;
    }

    // glm is column-major, but the tensor is symmetric so the layout doesn't matter.
    const glm::dmat3 tensor(xx, xy, xz,
                            xy, yy, yz,
                            xz, yz, zz);
    return glm::inverse(tensor);
  }
}

/**
 * @brief Get the name of the preprocessing stage.
 *
 * @return const char*
 */
const char* DtiTensorShStage::Name() const
{
  return "DTI tensor ODF to spherical harmonics";
}

/**
 * @brief Fit the tensor ODF (u^T D^-1 u)^(-3/2) of every voxel with a real SH series (lmax 8),
 *        normalised to unit integral (c00 = 1/(2 sqrt(pi))). Voxels with a non-finite,
 *        non-positive-definite or near-isotropic (FA <= 1e-3) tensor get all-zero coefficients.
 *        Stores the result, masked by a copy of the raw FA, in context.outputShVolume.
 *
 * @param context
 */
void DtiTensorShStage::Execute(MriPreprocessingContext& context) const
{
  const std::vector<glm::vec3> directions = IcosphereGeometry::GenerateDirections(kFitDirectionLevel);
  const std::vector<double> fitMatrix = RealSH::BuildFitMatrix(directions, RealSH::kSHMaxOrder, kFitRegularization);
  if (fitMatrix.empty())
  {
    context.report.warnings.push_back("SH fit matrix could not be built; spherical-harmonics glyphs are unavailable");
    return;
  }

  const DTIVolumeChannels& channels = context.outputDtiChannels;
  const VolumeMetadata metadata = channels.FA.GetMetadata();
  const int width = metadata.dimensions.x;
  const int height = metadata.dimensions.y;
  const int depth = metadata.dimensions.z;

  const std::vector<float>& dxx = channels.Dxx.GetVoxels();
  const std::vector<float>& dyy = channels.Dyy.GetVoxels();
  const std::vector<float>& dzz = channels.Dzz.GetVoxels();
  const std::vector<float>& dxy = channels.Dxy.GetVoxels();
  const std::vector<float>& dxz = channels.Dxz.GetVoxels();
  const std::vector<float>& dyz = channels.Dyz.GetVoxels();
  const std::vector<float>& fa = channels.FA.GetVoxels();

  constexpr size_t coefficientCount = static_cast<size_t>(RealSH::kSHMaxCoefficients);
  const size_t directionCount = directions.size();
  const double unitIntegralC00 = 1.0 / (2.0 * std::sqrt(glm::pi<double>()));

  auto shVolume = std::make_shared<SHVolume>();
  shVolume->dimensions = metadata.dimensions;
  shVolume->spacing = metadata.spacing;
  shVolume->lmax = RealSH::kSHMaxOrder;
  shVolume->coefficients.assign(static_cast<size_t>(width) * height * depth * coefficientCount, 0.0f);

  std::vector<glm::dvec3> unitDirections;
  unitDirections.reserve(directionCount);
  for (const glm::vec3& direction : directions)
  {
    unitDirections.push_back(glm::normalize(glm::dvec3(direction)));
  }

  std::vector<double> odf(directionCount);
  double coefficients[RealSH::kSHMaxCoefficients];

  for (int z = 0; z < depth; ++z)
  {
    context.ThrowIfCancelled();
    context.ReportStageFraction(static_cast<float>(z) / static_cast<float>(depth));

    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        const size_t i = VolumeData::FlatIndex(x, y, z, width, height);
        if (!std::isfinite(dxx[i]) || !std::isfinite(dyy[i]) || !std::isfinite(dzz[i]) ||
            !std::isfinite(dxy[i]) || !std::isfinite(dxz[i]) || !std::isfinite(dyz[i]) ||
            !std::isfinite(fa[i]) || fa[i] <= kMinFractionalAnisotropy)
        {
          continue;
        }

        const std::optional<glm::dmat3> inverseTensor = InvertPositiveDefinite(dxx[i], dyy[i], dzz[i], dxy[i], dxz[i], dyz[i]);
        if (!inverseTensor)
        {
          continue;
        }

        for (size_t d = 0; d < directionCount; ++d)
        {
          const glm::dvec3& u = unitDirections[d];
          odf[d] = std::pow(glm::dot(u, *inverseTensor * u), -1.5);
        }

        for (size_t k = 0; k < coefficientCount; ++k)
        {
          const double* fitRow = &fitMatrix[k * directionCount];
          double sum = 0.0;
          for (size_t d = 0; d < directionCount; ++d)
          {
            sum += fitRow[d] * odf[d];
          }
          coefficients[k] = sum;
        }

        if (!(coefficients[0] > 0.0) || !std::isfinite(coefficients[0]))
        {
          continue;
        }

        const double normalization = unitIntegralC00 / coefficients[0];
        float* voxelOut = &shVolume->coefficients[i * coefficientCount];
        bool allFinite = true;
        for (size_t k = 0; k < coefficientCount; ++k)
        {
          const double value = coefficients[k] * normalization;
          allFinite = allFinite && std::isfinite(value);
          voxelOut[k] = static_cast<float>(value);
        }
        if (!allFinite)
        {
          std::fill(voxelOut, voxelOut + coefficientCount, 0.0f);
        }
      }
    }
  }

  // Raw FA, copied here because the normalization stage later rescales the channel in place.
  shVolume->mask = std::make_shared<const VolumeData>(channels.FA);
  context.outputShVolume = std::move(shVolume);
}

/**
 * @brief Create a Dti Tensor Sh Stage object
 *
 * @return std::unique_ptr<IMriPreprocessingStage>
 */
std::unique_ptr<IMriPreprocessingStage> CreateDtiTensorShStage()
{
  return std::make_unique<DtiTensorShStage>();
}
