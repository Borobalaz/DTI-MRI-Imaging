#include "Preprocessing/stages/fodf/FodfFiberTractographyStage.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "FodfCsdMath.h"
#include "Preprocessing/MriFodfTractographySettings.h"
#include "Preprocessing/MriTractographySettings.h"
#include "Preprocessing/PreprocessingParallel.h"
#include "Preprocessing/stages/StreamlineTracing.h"
#include "Preprocessing/stages/fodf/FodfPreprocessingStages.h"
#include "foundation/log/EngineLog.h"
#include "scene/glyphs/SHVolume.h"
#include "scene/object/Mesh.h"
#include "scene/spherical_harmonics/RealSH.h"

namespace
{
  // Seeds traced per ParallelForSlices work item; results go to per-seed slots, so the output
  // order (and the mesh) does not depend on scheduling.
  constexpr int kSeedsPerChunk = 32;

  struct FodfTrackingParameters
  {
    float stepSizeVoxels = 0.0f;
    int maxStepsPerDirection = 0;
    size_t minPointsPerStreamline = 0;
    double maxAngleDegrees = 0.0;
    double amplitudeCutoff = 0.0;
  };

  // Trilinear interpolation of every SH coefficient at a voxel-space point (clamped into the volume).
  void SampleCoefficientsTrilinear(const SHVolume& sh, const glm::vec3& point, double* out)
  {
    const glm::ivec3& dims = sh.dimensions;
    const float x = std::clamp(point.x, 0.0f, static_cast<float>(dims.x - 1));
    const float y = std::clamp(point.y, 0.0f, static_cast<float>(dims.y - 1));
    const float z = std::clamp(point.z, 0.0f, static_cast<float>(dims.z - 1));

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = std::min(x0 + 1, dims.x - 1);
    const int y1 = std::min(y0 + 1, dims.y - 1);
    const int z1 = std::min(z0 + 1, dims.z - 1);

    const double tx = static_cast<double>(x - static_cast<float>(x0));
    const double ty = static_cast<double>(y - static_cast<float>(y0));
    const double tz = static_cast<double>(z - static_cast<float>(z0));

    const size_t coefficientCount = sh.CoefficientsPerVoxel();
    std::fill(out, out + coefficientCount, 0.0);

    const int cornerX[2] = {x0, x1};
    const int cornerY[2] = {y0, y1};
    const int cornerZ[2] = {z0, z1};
    const double weightX[2] = {1.0 - tx, tx};
    const double weightY[2] = {1.0 - ty, ty};
    const double weightZ[2] = {1.0 - tz, tz};
    for (int cz = 0; cz < 2; ++cz)
    {
      for (int cy = 0; cy < 2; ++cy)
      {
        for (int cx = 0; cx < 2; ++cx)
        {
          const double weight = weightX[cx] * weightY[cy] * weightZ[cz];
          if (weight == 0.0)
          {
            continue;
          }
          const float* corner = sh.VoxelCoefficients(cornerX[cx], cornerY[cy], cornerZ[cz]);
          for (size_t k = 0; k < coefficientCount; ++k)
          {
            out[k] += weight * static_cast<double>(corner[k]);
          }
        }
      }
    }
  }

  // The CSD mask: the nearest voxel must have been solved (finite c00 > 0).
  bool IsInsideFodfMask(const SHVolume& sh, const glm::vec3& point)
  {
    const glm::ivec3 nearest = glm::clamp(glm::ivec3(glm::round(point)), glm::ivec3(0), sh.dimensions - 1);
    const float c00 = sh.VoxelCoefficients(nearest.x, nearest.y, nearest.z)[0];
    return std::isfinite(c00) && c00 > 0.0f;
  }

  // One half of a streamline, starting at the seed and following the peak closest to the previous
  // direction; initialDirection is the unit physical-space direction of the first step.
  std::vector<glm::vec3> TraceHalfStreamline(const SHVolume& sh,
                                             const FodfCsdMath::PeakFinder& finder,
                                             FodfCsdMath::PeakFinder::Workspace& workspace,
                                             const FodfTrackingParameters& parameters,
                                             const glm::vec3& seedVoxel,
                                             const glm::dvec3& initialDirection)
  {
    std::vector<glm::vec3> points;
    double coefficients[RealSH::kSHMaxCoefficients];

    glm::vec3 position = seedVoxel;
    glm::dvec3 previousDirection = initialDirection;
    for (int step = 0; step < parameters.maxStepsPerDirection; ++step)
    {
      if (!StreamlineTracing::IsInBounds(position, sh.dimensions) || !IsInsideFodfMask(sh, position))
      {
        break;
      }

      SampleCoefficientsTrilinear(sh, position, coefficients);
      const std::optional<FodfCsdMath::SHPeak> peak = finder.FindClosest(
        coefficients, previousDirection, parameters.maxAngleDegrees, parameters.amplitudeCutoff, workspace);
      if (!peak)
      {
        break;
      }

      points.push_back(StreamlineTracing::VoxelToObjectSpace(position, sh.dimensions, sh.spacing));

      const glm::vec3 stepDirection = StreamlineTracing::ToIndexSpaceStepDirection(glm::vec3(peak->direction), sh.spacing);
      if (glm::length(stepDirection) <= 1e-6f)
      {
        break;
      }
      position += stepDirection * parameters.stepSizeVoxels;
      previousDirection = peak->direction;
    }

    return points;
  }

  // Both halves from the seed along +/- its strongest peak, joined; empty when the seed has no
  // peak above the cutoff or the joined line is shorter than the minimum.
  std::vector<glm::vec3> TraceStreamlineFromSeed(const SHVolume& sh,
                                                 const FodfCsdMath::PeakFinder& finder,
                                                 FodfCsdMath::PeakFinder::Workspace& workspace,
                                                 const FodfTrackingParameters& parameters,
                                                 const glm::vec3& seedVoxel)
  {
    if (!StreamlineTracing::IsInBounds(seedVoxel, sh.dimensions) || !IsInsideFodfMask(sh, seedVoxel))
    {
      return {};
    }

    double coefficients[RealSH::kSHMaxCoefficients];
    SampleCoefficientsTrilinear(sh, seedVoxel, coefficients);
    FodfCsdMath::SHPeak strongest;
    if (finder.Find(coefficients, 1, workspace, &strongest) == 0 || strongest.amplitude < parameters.amplitudeCutoff)
    {
      return {};
    }

    const std::vector<glm::vec3> forward =
      TraceHalfStreamline(sh, finder, workspace, parameters, seedVoxel, strongest.direction);
    const std::vector<glm::vec3> backward =
      TraceHalfStreamline(sh, finder, workspace, parameters, seedVoxel, -strongest.direction);

    std::vector<glm::vec3> streamline = StreamlineTracing::JoinBidirectional(backward, forward);
    if (streamline.size() < parameters.minPointsPerStreamline)
    {
      return {};
    }
    return streamline;
  }
}

/**
 * @brief Get the name of the preprocessing stage.
 *
 * @return const char*
 */
const char* FodfFiberTractographyStage::Name() const
{
  return "fODF fiber tractography";
}

/**
 * @brief Trace fODF peak-following streamlines from the DTI seeds and store their tube mesh in
 *        context.outputFodfStreamlineMesh.
 *
 * @param context
 */
void FodfFiberTractographyStage::Execute(MriPreprocessingContext& context) const
{
  const std::shared_ptr<const SHVolume> sh = context.outputFodfShVolume;
  if (!sh || !sh->IsWellFormed())
  {
    throw std::runtime_error("fODF tractography: no valid fODF SH volume; fODF CSD must run first.");
  }
  if (!(context.fodfAbsoluteAmplitudeCutoff > 0.0f))
  {
    throw std::runtime_error("fODF tractography: the peak amplitude cutoff is not positive; "
                             "fODF peak extraction must run first with a relative cutoff above 0.");
  }

  context.outputFodfStreamlineMesh = nullptr;
  const std::vector<glm::vec3>& seeds = context.tractographySeedsVoxelSpace;
  if (seeds.empty())
  {
    context.report.warnings.push_back("fODF tractography: no tractography seeds; no fODF streamlines were traced.");
    return;
  }

  const MriTractographySettings defaultSharedSettings;
  const MriTractographySettings& sharedSettings =
    context.request.tractographySettings ? *context.request.tractographySettings : defaultSharedSettings;
  const MriFodfTractographySettings fodfSettings =
    context.request.fodfTractographySettings ? *context.request.fodfTractographySettings : MriFodfTractographySettings{};

  FodfTrackingParameters parameters;
  parameters.stepSizeVoxels = sharedSettings.GetStepSizeVoxels();
  parameters.maxStepsPerDirection = sharedSettings.GetMaxStepsPerStreamline();
  parameters.minPointsPerStreamline = sharedSettings.GetMinPointsPerStreamline();
  parameters.maxAngleDegrees = static_cast<double>(fodfSettings.GetMaxAngleDegrees());
  parameters.amplitudeCutoff = static_cast<double>(context.fodfAbsoluteAmplitudeCutoff);

  const FodfCsdMath::PeakFinder finder(FodfCsdMath::BuildPeakSampleDirections(), sh->lmax,
                                       FodfCsdMath::kPeakNeighbourAngleDeg, FodfCsdMath::kPeakSeparationDeg);

  const int seedCount = static_cast<int>(seeds.size());
  const int chunkCount = (seedCount + kSeedsPerChunk - 1) / kSeedsPerChunk;
  std::vector<std::vector<glm::vec3>> streamlinesPerSeed(seeds.size());
  ParallelForSlices(context, chunkCount, [&](int chunk)
  {
    FodfCsdMath::PeakFinder::Workspace workspace(finder);
    const int firstSeed = chunk * kSeedsPerChunk;
    const int endSeed = std::min(firstSeed + kSeedsPerChunk, seedCount);
    for (int seedIndex = firstSeed; seedIndex < endSeed; ++seedIndex)
    {
      const size_t slot = static_cast<size_t>(seedIndex);
      streamlinesPerSeed[slot] = TraceStreamlineFromSeed(*sh, finder, workspace, parameters, seeds[slot]);
    }
  });

  std::vector<std::vector<glm::vec3>> streamlines;
  for (std::vector<glm::vec3>& streamline : streamlinesPerSeed)
  {
    if (!streamline.empty())
    {
      streamlines.push_back(std::move(streamline));
    }
  }

  EngineLog::Message("fODF tractography: " + std::to_string(streamlines.size()) + " streamlines from " +
                     std::to_string(seeds.size()) + " seeds", "FodfTractography");

  context.outputFodfStreamlineMesh = StreamlineTracing::BuildStreamlineTubeMesh(
    streamlines, sharedSettings.GetTubeRadius(), sharedSettings.GetTubeRadialSegments());
  if (!context.outputFodfStreamlineMesh)
  {
    context.report.warnings.push_back("fODF tractography produced no streamlines.");
  }
}

/**
 * @brief Create a Fodf Fiber Tractography Stage object
 *
 * @return std::unique_ptr<IMriPreprocessingStage>
 */
std::unique_ptr<IMriPreprocessingStage> CreateFodfFiberTractographyStage()
{
  return std::make_unique<FodfFiberTractographyStage>();
}
