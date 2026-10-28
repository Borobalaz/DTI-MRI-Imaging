#include "Preprocessing/stages/fodf/FodfResponseEstimationStage.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "FodfCsdMath.h"
#include "Preprocessing/MriFodfSettings.h"
#include "Preprocessing/stages/fodf/FodfPreprocessingStages.h"
#include "foundation/log/EngineLog.h"
#include "scene/volume/VolumeData.h"
#include "scene/volume/VolumeSeriesData.h"

namespace
{
  constexpr double kMinEigenvectorLength = 0.5;
  constexpr double kRayleighRelativeTolerance = 0.01;

  struct ResponseCandidate
  {
    float fa;
    size_t index;
  };

  bool IsPositiveDefinite(double xx, double yy, double zz, double xy, double xz, double yz)
  {
    const double minor2 = xx * yy - xy * xy;
    const double determinant = xx * (yy * zz - yz * yz) - xy * (xy * zz - yz * xz) + xz * (xy * yz - yy * xz);
    return xx > 0.0 && minor2 > 0.0 && determinant > 0.0;
  }

  // Raw mean of the b0 frames at voxel i (no clamp), so invalid voxels stay detectable.
  double MeanB0(const std::vector<float>& allSignals, const std::vector<size_t>& b0Frames, size_t spatialCount, size_t i)
  {
    double sum = 0.0;
    for (const size_t frame : b0Frames)
    {
      sum += static_cast<double>(allSignals[frame * spatialCount + i]);
    }
    return sum / static_cast<double>(b0Frames.size());
  }
}

/**
 * @brief Get the name of the preprocessing stage.
 *
 * @return const char*
 */
const char* FodfResponseEstimationStage::Name() const
{
  return "fODF response estimation";
}

/**
 * @brief Select the top-N raw-FA voxels (optional FA floor, finite signal, positive-definite tensor,
 *        eigenvector consistent with L1), jointly fit the axially symmetric response to their
 *        eigenvector-aligned normalised shell signals and store it in context.outputFodfResponse.
 *
 * @param context
 */
void FodfResponseEstimationStage::Execute(MriPreprocessingContext& context) const
{
  const MriFodfSettings settings = context.request.fodfSettings ? *context.request.fodfSettings : MriFodfSettings{};
  const int lmax = settings.GetLmax();
  const size_t responseVoxelCount = static_cast<size_t>(settings.GetResponseVoxelCount());
  const float faFloor = settings.GetResponseFaFloor();

  const DwiShellSelection& selection = context.shellSelection;
  if (!context.dwiSeries || selection.shellFrames.empty())
  {
    throw std::runtime_error(
      "fODF response estimation: DWI series or shell selection missing; DWI input validation and DWI shell "
      "selection must run first.");
  }

  const VolumeSeriesData& dwiSeries = *context.dwiSeries;
  const DTIVolumeChannels& channels = context.outputDtiChannels;
  const VolumeMetadata metadata = channels.FA.GetMetadata();
  if (metadata.dimensions != dwiSeries.GetDimensions())
  {
    throw std::runtime_error("fODF response estimation: DWI and DTI channel dimensions differ.");
  }

  const size_t shellCount = selection.shellFrames.size();
  if (shellCount < FodfCsdMath::kMinResponseDirections)
  {
    throw std::runtime_error("fODF response estimation: shell has " + std::to_string(shellCount) +
                             " directions; at least 6 are needed.");
  }

  const int width = metadata.dimensions.x;
  const int height = metadata.dimensions.y;
  const int depth = metadata.dimensions.z;
  const size_t spatialCount = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
  const std::vector<float>& allSignals = dwiSeries.GetVoxels();

  const std::vector<float>& dxx = channels.Dxx.GetVoxels();
  const std::vector<float>& dyy = channels.Dyy.GetVoxels();
  const std::vector<float>& dzz = channels.Dzz.GetVoxels();
  const std::vector<float>& dxy = channels.Dxy.GetVoxels();
  const std::vector<float>& dxz = channels.Dxz.GetVoxels();
  const std::vector<float>& dyz = channels.Dyz.GetVoxels();
  const std::vector<float>& evx = channels.EVx.GetVoxels();
  const std::vector<float>& evy = channels.EVy.GetVoxels();
  const std::vector<float>& evz = channels.EVz.GetVoxels();
  const std::vector<float>& fa = channels.FA.GetVoxels();
  const std::vector<float>& l1 = channels.L1.GetVoxels();

  // Candidate scan: every voxel whose tensor, eigenvector and signal can enter the response fit.
  std::vector<ResponseCandidate> candidates;
  size_t floorPass = 0;
  size_t eigenvectorMismatches = 0;
  for (int z = 0; z < depth; ++z)
  {
    context.ThrowIfCancelled();
    context.ReportStageFraction(0.9f * static_cast<float>(z) / static_cast<float>(depth));

    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        const size_t i = VolumeData::FlatIndex(x, y, z, width, height);
        if (!std::isfinite(dxx[i]) || !std::isfinite(dyy[i]) || !std::isfinite(dzz[i]) ||
            !std::isfinite(dxy[i]) || !std::isfinite(dxz[i]) || !std::isfinite(dyz[i]) ||
            !std::isfinite(fa[i]) || !std::isfinite(l1[i]) ||
            !std::isfinite(evx[i]) || !std::isfinite(evy[i]) || !std::isfinite(evz[i]))
        {
          continue;
        }

        const double xx = dxx[i], yy = dyy[i], zz = dzz[i], xy = dxy[i], xz = dxz[i], yz = dyz[i];
        if (!IsPositiveDefinite(xx, yy, zz, xy, xz, yz))
        {
          continue;
        }

        const glm::dvec3 eigenvector(evx[i], evy[i], evz[i]);
        const double eigenvectorLength = glm::length(eigenvector);
        if (!(eigenvectorLength >= kMinEigenvectorLength))
        {
          continue;
        }

        // Rayleigh guard: the stored eigenvector must actually belong to L1 (power iteration can
        // land on another axis).
        const glm::dvec3 vn = eigenvector / eigenvectorLength;
        const glm::dvec3 dv(xx * vn.x + xy * vn.y + xz * vn.z,
                            xy * vn.x + yy * vn.y + yz * vn.z,
                            xz * vn.x + yz * vn.y + zz * vn.z);
        const double rayleigh = glm::dot(vn, dv);
        const double principal = static_cast<double>(l1[i]);
        if (!(std::abs(rayleigh - principal) <= kRayleighRelativeTolerance * std::abs(principal)))
        {
          ++eigenvectorMismatches;
          continue;
        }

        const double s0 = MeanB0(allSignals, selection.b0Frames, spatialCount, i);
        if (!std::isfinite(s0) || !(s0 > 0.0))
        {
          continue;
        }

        const bool shellFinite = std::all_of(selection.shellFrames.begin(), selection.shellFrames.end(),
                                             [&](size_t frame) { return std::isfinite(allSignals[frame * spatialCount + i]); });
        if (!shellFinite)
        {
          continue;
        }

        candidates.push_back(ResponseCandidate{fa[i], i});
        if (fa[i] >= faFloor)
        {
          ++floorPass;
        }
      }
    }
  }

  // Selection: top-N by FA (ties by index), restricted to the FA floor when enough voxels pass it.
  std::vector<ResponseCandidate> pool;
  if (floorPass >= static_cast<size_t>(FodfCsdMath::kMinResponseVoxels))
  {
    pool.reserve(floorPass);
    for (const ResponseCandidate& candidate : candidates)
    {
      if (candidate.fa >= faFloor)
      {
        pool.push_back(candidate);
      }
    }
  }
  else
  {
    context.report.warnings.push_back(
      "fODF response: only " + std::to_string(floorPass) + " voxels have FA >= " + FodfCsdMath::FormatFixed(faFloor, 2) +
      "; using the top " + std::to_string(responseVoxelCount) + " voxels by FA without the floor.");
    pool = std::move(candidates);
  }

  if (pool.size() < static_cast<size_t>(FodfCsdMath::kMinResponseVoxels))
  {
    throw std::runtime_error(
      "fODF response estimation: only " + std::to_string(pool.size()) +
      " voxels qualify (finite signal, mean b0 > 0, positive-definite tensor); at least 10 are needed.");
  }

  const size_t selectedCount = std::min(responseVoxelCount, pool.size());
  std::partial_sort(pool.begin(), pool.begin() + static_cast<std::ptrdiff_t>(selectedCount), pool.end(),
                    [](const ResponseCandidate& lhs, const ResponseCandidate& rhs)
                    {
                      if (lhs.fa != rhs.fa)
                      {
                        return lhs.fa > rhs.fa;
                      }
                      return lhs.index < rhs.index;
                    });
  pool.resize(selectedCount);

  // Joint-fit inputs. Gradient directions stay in the bvec-file frame, the same frame as D and the EVs.
  std::vector<glm::dvec3> shellDirs;
  shellDirs.reserve(shellCount);
  for (const size_t frame : selection.shellFrames)
  {
    shellDirs.push_back(glm::normalize(glm::dvec3(context.gradientDirections[frame])));
  }

  std::vector<double> signals;
  signals.reserve(selectedCount * shellCount);
  std::vector<glm::dvec3> axes;
  axes.reserve(selectedCount);
  double faSum = 0.0;
  for (const ResponseCandidate& candidate : pool)
  {
    const size_t i = candidate.index;
    const double s0 = MeanB0(allSignals, selection.b0Frames, spatialCount, i);
    for (const size_t frame : selection.shellFrames)
    {
      signals.push_back(static_cast<double>(allSignals[frame * spatialCount + i]) / s0);
    }
    axes.push_back(glm::normalize(glm::dvec3(evx[i], evy[i], evz[i])));
    faSum += static_cast<double>(candidate.fa);
  }
  const double meanFa = faSum / static_cast<double>(selectedCount);

  context.ThrowIfCancelled();
  const FodfCsdMath::AxialResponse response = FodfCsdMath::FitAxialResponse(shellDirs, signals, axes, lmax);
  if (!response.IsValid())
  {
    throw std::runtime_error("fODF response estimation: " + response.error + ".");
  }
  if (response.voxelCount < static_cast<size_t>(FodfCsdMath::kMinResponseVoxels))
  {
    throw std::runtime_error("fODF response estimation: only " + std::to_string(response.voxelCount) +
                             " voxels entered the joint fit; at least 10 are needed.");
  }

  const bool rotationalFinite = std::all_of(response.rotational.begin(), response.rotational.end(),
                                            [](double value) { return std::isfinite(value); });
  if (!rotationalFinite || !(response.rotational[0] > 0.0))
  {
    throw std::runtime_error("fODF response estimation: response coefficients are invalid (r_0 = " +
                             FodfCsdMath::FormatScientific(response.rotational[0]) +
                             "); check the b0 frames and gradient table.");
  }
  if (lmax >= 2 && response.rotational[1] >= 0.0)
  {
    context.report.warnings.push_back(
      "fODF response: r_2 >= 0 (expected negative for a fibre-aligned response); the principal eigenvectors and "
      "gradient directions may not share a frame.");
  }

  FodfResponseFunction output;
  output.bValue = selection.shellBValue;
  output.lmax = lmax;
  for (size_t k = 0; k < response.rotational.size(); ++k)
  {
    output.zonalCoefficients.push_back(static_cast<float>(response.zonal[k]));
    output.rotationalCoefficients.push_back(static_cast<float>(response.rotational[k]));
  }
  output.voxelCount = response.voxelCount;
  output.meanFa = static_cast<float>(meanFa);

  std::string coefficientsText;
  for (size_t k = 0; k < response.rotational.size(); ++k)
  {
    coefficientsText += (k == 0 ? "" : ", ") + FodfCsdMath::FormatScientific(response.rotational[k]);
  }
  std::string logLine =
    "fODF response (b=" + std::to_string(std::lround(selection.shellBValue)) + ", lmax " + std::to_string(lmax) + ", " +
    std::to_string(response.voxelCount) + " voxels, " + std::to_string(response.rowCount) + " rows, mean FA " +
    FodfCsdMath::FormatFixed(meanFa, 3) + "): r_l = [" + coefficientsText + "]";
  if (eigenvectorMismatches > 0 || response.failedVoxelCount > 0)
  {
    logLine += " (skipped: " + std::to_string(eigenvectorMismatches) + " eigenvector mismatches, " +
               std::to_string(response.failedVoxelCount) + " invalid inputs)";
  }
  EngineLog::Message(logLine, "FodfCsd");

  context.outputFodfResponse = std::move(output);
  context.ReportStageFraction(1.0f);
}

/**
 * @brief Create a Fodf Response Estimation Stage object
 *
 * @return std::unique_ptr<IMriPreprocessingStage>
 */
std::unique_ptr<IMriPreprocessingStage> CreateFodfResponseEstimationStage()
{
  return std::make_unique<FodfResponseEstimationStage>();
}
