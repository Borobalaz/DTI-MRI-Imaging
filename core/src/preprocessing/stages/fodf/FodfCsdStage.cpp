#include "Preprocessing/stages/fodf/FodfCsdStage.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "FodfCsdMath.h"
#include "Preprocessing/MriFodfSettings.h"
#include "Preprocessing/PreprocessingParallel.h"
#include "Preprocessing/stages/fodf/FodfPreprocessingStages.h"
#include "foundation/log/EngineLog.h"
#include "scene/glyphs/SHVolume.h"
#include "scene/spherical_harmonics/RealSH.h"
#include "scene/volume/VolumeData.h"
#include "scene/volume/VolumeSeriesData.h"

namespace
{
  constexpr double kNotConvergedWarningFraction = 0.01;

  // Per-slice QC counters; each slice is written by exactly one worker.
  struct SliceStats
  {
    size_t processed = 0;
    size_t converged = 0;
    size_t notConverged = 0;
    size_t underdetermined = 0;
    size_t solveFailed = 0;
    std::uint64_t iterationSum = 0;
    int iterationMax = 0;
  };
}

/**
 * @brief Get the name of the preprocessing stage.
 *
 * @return const char*
 */
const char* FodfCsdStage::Name() const
{
  return "fODF constrained spherical deconvolution";
}

/**
 * @brief Run CSD on every voxel with a finite signal and mean b0 > 0, using the response from
 *        FodfResponseEstimationStage, and store the fODF SH volume in context.outputFodfShVolume.
 *
 * @param context
 */
void FodfCsdStage::Execute(MriPreprocessingContext& context) const
{
  const MriFodfSettings settings = context.request.fodfSettings ? *context.request.fodfSettings : MriFodfSettings{};
  const int lmax = settings.GetLmax();
  const double tau = static_cast<double>(settings.GetNegativityThresholdTau());
  const double lambda = static_cast<double>(settings.GetLambda());
  const int maxIterations = settings.GetMaxIterations();

  if (!context.outputFodfResponse.IsValid())
  {
    throw std::runtime_error("fODF CSD: no valid response function; fODF response estimation must run first.");
  }
  const DwiShellSelection& selection = context.shellSelection;
  if (!context.dwiSeries || selection.shellFrames.empty())
  {
    throw std::runtime_error(
      "fODF CSD: DWI series or shell selection missing; DWI input validation and DWI shell selection must run first.");
  }

  const VolumeSeriesData& dwiSeries = *context.dwiSeries;
  const VolumeMetadata metadata = context.outputDtiChannels.FA.GetMetadata();
  if (metadata.dimensions != dwiSeries.GetDimensions())
  {
    throw std::runtime_error("fODF CSD: DWI and DTI channel dimensions differ.");
  }

  const size_t shellCount = selection.shellFrames.size();
  std::vector<glm::vec3> shellDirsF;
  std::vector<glm::dvec3> shellDirsD;
  shellDirsF.reserve(shellCount);
  shellDirsD.reserve(shellCount);
  for (const size_t frame : selection.shellFrames)
  {
    const glm::dvec3 direction = glm::normalize(glm::dvec3(context.gradientDirections[frame]));
    shellDirsD.push_back(direction);
    shellDirsF.push_back(glm::vec3(direction));
  }

  if (!SelfTestCsd(shellDirsF, selection.shellBValue, lmax))
  {
    context.report.warnings.push_back(
      "fODF CSD self-test failed on this gradient scheme (see log); fODF peaks may be unreliable.");
  }
  context.ThrowIfCancelled();

  const std::vector<float>& rotationalF = context.outputFodfResponse.rotationalCoefficients;
  const std::vector<double> rotational(rotationalF.begin(), rotationalF.end());
  const FodfCsdMath::CsdSolver solver(shellDirsD, rotational,
                                      FodfCsdMath::CsdSolver::Settings{lmax, tau, lambda, maxIterations});
  const int coefficientCount = solver.GetCoefficientCount();
  if (solver.IsSuperResolved())
  {
    EngineLog::Message("fODF CSD: super-resolution (" + std::to_string(coefficientCount) + " coefficients from " +
                       std::to_string(shellCount) + " shell directions)", "FodfCsd");
  }

  const int width = metadata.dimensions.x;
  const int height = metadata.dimensions.y;
  const int depth = metadata.dimensions.z;
  const size_t spatialCount = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
  constexpr size_t kOutputStride = static_cast<size_t>(RealSH::kSHMaxCoefficients);

  auto sh = std::make_shared<SHVolume>();
  sh->dimensions = metadata.dimensions;
  sh->spacing = metadata.spacing;
  sh->lmax = RealSH::kSHMaxOrder;
  sh->coefficients.assign(sh->VoxelCount() * kOutputStride, 0.0f);

  std::vector<SliceStats> stats(static_cast<size_t>(depth));
  const std::vector<float>& allSignals = dwiSeries.GetVoxels();
  std::vector<float>& coefficients = sh->coefficients;

  ParallelForSlices(context, depth, [&](int z)
  {
    FodfCsdMath::CsdSolver::Workspace workspace(solver);
    std::vector<double> signal(shellCount);
    double f[RealSH::kSHMaxCoefficients];
    SliceStats& sliceStats = stats[static_cast<size_t>(z)];

    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        const size_t i = VolumeData::FlatIndex(x, y, z, width, height);

        double s0Sum = 0.0;
        bool finite = true;
        for (const size_t frame : selection.b0Frames)
        {
          const float value = allSignals[frame * spatialCount + i];
          finite = finite && std::isfinite(value);
          s0Sum += static_cast<double>(value);
        }
        const double s0 = s0Sum / static_cast<double>(selection.b0Frames.size());
        if (!finite || !std::isfinite(s0) || !(s0 > 0.0))
        {
          continue;
        }

        for (size_t j = 0; j < shellCount; ++j)
        {
          const float value = allSignals[selection.shellFrames[j] * spatialCount + i];
          finite = finite && std::isfinite(value);
          signal[j] = static_cast<double>(value) / s0;
        }
        if (!finite)
        {
          continue;
        }

        const FodfCsdMath::CsdSolver::VoxelResult result = solver.SolveVoxel(signal.data(), workspace, f);
        ++sliceStats.processed;
        sliceStats.iterationSum += static_cast<std::uint64_t>(result.iterations);
        sliceStats.iterationMax = std::max(sliceStats.iterationMax, result.iterations);

        const bool coefficientsFinite = std::all_of(f, f + coefficientCount, [](double value) { return std::isfinite(value); });
        if (!coefficientsFinite)
        {
          ++sliceStats.solveFailed;
          continue;
        }

        switch (result.status)
        {
        case FodfCsdMath::CsdSolver::VoxelStatus::Converged: ++sliceStats.converged; break;
        case FodfCsdMath::CsdSolver::VoxelStatus::NotConverged: ++sliceStats.notConverged; break;
        case FodfCsdMath::CsdSolver::VoxelStatus::Underdetermined: ++sliceStats.underdetermined; break;
        case FodfCsdMath::CsdSolver::VoxelStatus::SolveFailed: ++sliceStats.solveFailed; break;
        }

        // RealSH nesting: indices 0..n-1 are exactly the lmax-L coefficients; n..44 stay zero.
        float* voxelOut = &coefficients[i * kOutputStride];
        for (int k = 0; k < coefficientCount; ++k)
        {
          voxelOut[k] = static_cast<float>(f[k]);
        }
      }
    }
  });

  if (context.outputShVolume && context.outputShVolume->mask)
  {
    sh->mask = context.outputShVolume->mask;
  }
  else
  {
    // Raw FA, copied because the normalization stage later rescales the channel in place.
    sh->mask = std::make_shared<const VolumeData>(context.outputDtiChannels.FA);
  }
  context.outputFodfShVolume = std::move(sh);

  SliceStats total;
  for (const SliceStats& slice : stats)
  {
    total.processed += slice.processed;
    total.converged += slice.converged;
    total.notConverged += slice.notConverged;
    total.underdetermined += slice.underdetermined;
    total.solveFailed += slice.solveFailed;
    total.iterationSum += slice.iterationSum;
    total.iterationMax = std::max(total.iterationMax, slice.iterationMax);
  }

  if (total.processed == 0)
  {
    throw std::runtime_error("fODF CSD: no voxel has a finite signal with mean b0 > 0.");
  }

  const double meanIterations = static_cast<double>(total.iterationSum) / static_cast<double>(total.processed);
  EngineLog::Message(
    "fODF CSD: " + std::to_string(total.processed) + " voxels (lmax " + std::to_string(lmax) + ", lambda_eff " +
    FodfCsdMath::FormatScientific(solver.GetEffectiveLambda()) + ", " + std::to_string(solver.GetHrCount()) +
    " HR dirs), iterations mean " + FodfCsdMath::FormatFixed(meanIterations, 2) + " max " +
    std::to_string(total.iterationMax) + "; converged " + std::to_string(total.converged) + ", not converged " +
    std::to_string(total.notConverged) + ", underdetermined " + std::to_string(total.underdetermined) +
    ", solve failed " + std::to_string(total.solveFailed),
    "FodfCsd");

  if (static_cast<double>(total.notConverged) > kNotConvergedWarningFraction * static_cast<double>(total.processed))
  {
    context.report.warnings.push_back(
      "fODF CSD: " + std::to_string(total.notConverged) + " of " + std::to_string(total.processed) +
      " voxels did not converge within " + std::to_string(maxIterations) + " iterations.");
  }
  if (total.underdetermined > 0)
  {
    context.report.warnings.push_back(
      "fODF CSD: " + std::to_string(total.underdetermined) +
      " voxels were underdetermined (shell directions + negativity constraints < " + std::to_string(coefficientCount) +
      " coefficients); their fODF is the last estimate.");
  }
  if (total.solveFailed > 0)
  {
    context.report.warnings.push_back(
      "fODF CSD: " + std::to_string(total.solveFailed) +
      " voxels hit a failed solve; their fODF is the last valid estimate (or zero if non-finite).");
  }
}

/**
 * @brief Create a Fodf Csd Stage object
 *
 * @return std::unique_ptr<IMriPreprocessingStage>
 */
std::unique_ptr<IMriPreprocessingStage> CreateFodfCsdStage()
{
  return std::make_unique<FodfCsdStage>();
}
