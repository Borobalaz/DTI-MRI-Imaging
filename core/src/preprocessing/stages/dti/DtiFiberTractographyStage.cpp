#include "Preprocessing/stages/dti/DtiFiberTractographyStage.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "scene/object/Mesh.h"
#include "Preprocessing/MriTractographySettings.h"
#include "Preprocessing/stages/StreamlineTracing.h"
#include "foundation/log/EngineLog.h"
#include "scene/volume/VolumeData.h"

namespace
{
  // Seed selection is a cheap strided scan; tracing dominates the stage's run time.
  constexpr float kSeedSelectionFractionShare = 0.1f;
}

struct DtiFiberTractographyStage::TractographySettings
{
  float faSeedThreshold = 0.4f;
  float faStopThreshold = 0.3f;
  float l1StopThreshold = 1e-6f;
  float stepSizeVoxels = 0.3f;
  int maxStepsPerStreamline = 250;
  int seedStride = 1;
  size_t maxSeeds = 3000;
  size_t minPointsPerStreamline = 15;

  // Geometry settings
  float tubeRadius = 0.001f;
  unsigned int tubeRadialSegments = 3;
};

/**
 * @brief Build tractography settings from the preprocessing request.
 * 
 * @param context Preprocessing context containing the request.
 * @return Tractography settings copied from the inspectable request object.
 */
DtiFiberTractographyStage::TractographySettings DtiFiberTractographyStage::BuildTractographySettings(
    const MriPreprocessingContext &context)
{
  TractographySettings settings;
  if (!context.request.tractographySettings)
  {
    return settings;
  }

  settings.faSeedThreshold = context.request.tractographySettings->GetFaSeedThreshold();
  settings.faStopThreshold = context.request.tractographySettings->GetFaStopThreshold();
  settings.l1StopThreshold = context.request.tractographySettings->GetL1StopThreshold();
  settings.stepSizeVoxels = context.request.tractographySettings->GetStepSizeVoxels();
  settings.maxStepsPerStreamline = context.request.tractographySettings->GetMaxStepsPerStreamline();
  settings.seedStride = context.request.tractographySettings->GetSeedStride();
  settings.maxSeeds = context.request.tractographySettings->GetMaxSeeds();
  settings.minPointsPerStreamline = context.request.tractographySettings->GetMinPointsPerStreamline();
  settings.tubeRadius = context.request.tractographySettings->GetTubeRadius();
  settings.tubeRadialSegments = context.request.tractographySettings->GetTubeRadialSegments();
  return settings;
}

/**
 * @brief Sample the principal eigenvector channels using trilinear interpolation.
 * 
 * @param voxelPosition Position in voxel coordinates.
 * @param dims Volume dimensions.
 * @param evx X component channel.
 * @param evy Y component channel.
 * @param evz Z component channel.
 * @return Interpolated principal direction vector.
 */
glm::vec3 DtiFiberTractographyStage::SampleTrilinearDirection(const glm::vec3 &voxelPosition,
                                                             const glm::ivec3 &dims,
                                                             const std::vector<float> &evx,
                                                             const std::vector<float> &evy,
                                                             const std::vector<float> &evz)
{
  return glm::vec3(
      StreamlineTracing::SampleTrilinear(evx, voxelPosition, dims),
      StreamlineTracing::SampleTrilinear(evy, voxelPosition, dims),
      StreamlineTracing::SampleTrilinear(evz, voxelPosition, dims));
}

/**
 * @brief Select seed points for streamline tracing using interpolated FA and L1 values.
 * 
 * @param context Used for cancellation checks and progress reporting only.
 * @param faVolume FA volume.
 * @param l1Volume L1 volume.
 * @param settings Tractography settings.
 * @return Seed positions in voxel coordinates.
 */
std::vector<glm::vec3> DtiFiberTractographyStage::SelectSeedPoints(const MriPreprocessingContext &context,
                                                                    const VolumeData &faVolume,
                                                                    const VolumeData &l1Volume,
                                                                    const TractographySettings &settings)
{
  const VolumeMetadata &metadata = faVolume.GetMetadata();
  const glm::ivec3 dims = metadata.dimensions;

  std::vector<glm::vec3> seeds;
  seeds.reserve(settings.maxSeeds);

  // Iterate through every strideth voxel
  const int stride = std::max(settings.seedStride, 1);
  for (int z = 0; z < dims.z; z += stride)
  {
    context.ThrowIfCancelled();
    context.ReportStageFraction(
        kSeedSelectionFractionShare * static_cast<float>(z) / static_cast<float>(dims.z));

    for (int y = 0; y < dims.y; y += stride)
    {
      for (int x = 0; x < dims.x; x += stride)
      {
        const glm::vec3 samplePoint(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        const float fa = StreamlineTracing::SampleTrilinear(faVolume.GetVoxels(), samplePoint, dims);
        const float l1 = StreamlineTracing::SampleTrilinear(l1Volume.GetVoxels(), samplePoint, dims);
        if (!std::isfinite(fa) || !std::isfinite(l1))
        {
          continue;
        }

        if (fa < settings.faSeedThreshold || l1 < settings.l1StopThreshold)
        {
          continue;
        }

        // If voxel has sufficient anisotropy and diffusion strength, add to seed list
        seeds.emplace_back(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        if (seeds.size() >= settings.maxSeeds)
        {
          return seeds;
        }
      }
    }
  }

  return seeds;
}

/**
 * @brief Sample the principal eigenvector at a given voxel-space position.
 * 
 * @param voxelPosition Sample position in voxel coordinates.
 * @param dims Volume dimensions.
 * @param spacing Voxel spacing.
 * @param evx X component channel.
 * @param evy Y component channel.
 * @param evz Z component channel.
 * @param previousDirection Previous direction used to preserve orientation continuity.
 * @return Normalized object-space direction vector.
 */
glm::vec3 DtiFiberTractographyStage::SamplePrincipalDirection(const glm::vec3 &voxelPosition,
                                                             const glm::ivec3 &dims,
                                                             const glm::vec3 &spacing,
                                                             const std::vector<float> &evx,
                                                             const std::vector<float> &evy,
                                                             const std::vector<float> &evz,
                                                             const glm::vec3 &previousDirection)
{
  glm::vec3 direction = SampleTrilinearDirection(voxelPosition, dims, evx, evy, evz);
  if (!std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z))
  {
    return glm::vec3(0.0f);
  }

  // Normalize direction
  const float rawLength = glm::length(direction);
  if (rawLength <= 1e-6f)
  {
    return glm::vec3(0.0f);
  }

  direction /= rawLength;

  // Preserve orientation continuity to avoid local sign flips in principal eigenvectors.
  if (glm::length(previousDirection) > 1e-6f && glm::dot(direction, previousDirection) < 0.0f)
  {
    direction = -direction;
  }

  // Normalize again in account to volume spacing to get correct step sizes in object space
  return StreamlineTracing::ToIndexSpaceStepDirection(direction, spacing);
}

/**
 * @brief Trace one half of a streamline from a seed point.
 * 
 * @param seedVoxel Seed position in voxel coordinates.
 * @param dims Volume dimensions.
 * @param spacing Voxel spacing.
 * @param faVoxels FA voxel buffer.
 * @param l1Voxels L1 voxel buffer.
 * @param evx X component channel.
 * @param evy Y component channel.
 * @param evz Z component channel.
 * @param settings Tractography settings.
 * @param initialReferenceDirection Orients the first step (the eigenvector's sign is arbitrary):
 *        zero follows the sampled sign, the negated first forward step traces the other way.
 * @param firstStepDirection Receives the voxel-space direction of the first step, or zero if
 *        no step was taken.
 * @return Streamline points in object space coordinates, starting at the seed.
 */
std::vector<glm::vec3> DtiFiberTractographyStage::TraceStreamlineFromSeed(const glm::vec3 &seedVoxel,
                                                                          const glm::ivec3 &dims,
                                                                          const glm::vec3 &spacing,
                                                                          const std::vector<float> &faVoxels,
                                                                          const std::vector<float> &l1Voxels,
                                                                          const std::vector<float> &evx,
                                                                          const std::vector<float> &evy,
                                                                          const std::vector<float> &evz,
                                                                          const TractographySettings &settings,
                                                                          const glm::vec3 &initialReferenceDirection,
                                                                          glm::vec3 &firstStepDirection)
{
  std::vector<glm::vec3> points;
  points.reserve(static_cast<size_t>(settings.maxStepsPerStreamline));

  glm::vec3 currentPosition = seedVoxel;
  glm::vec3 previousDirection = initialReferenceDirection;
  firstStepDirection = glm::vec3(0.0f);

  // Iterate until stopping criteria
  for (int step = 0; step < settings.maxStepsPerStreamline; ++step)
  {
    if (!StreamlineTracing::IsInBounds(currentPosition, dims))
    {
      break;
    }

    // Sample voxel data at current position using trilinear interpolation.
    const float fa = StreamlineTracing::SampleTrilinear(faVoxels, currentPosition, dims);
    const float l1 = StreamlineTracing::SampleTrilinear(l1Voxels, currentPosition, dims);

    if (!std::isfinite(fa) || !std::isfinite(l1) ||
        fa < settings.faStopThreshold || l1 < settings.l1StopThreshold)
    {
      break;
    }

    // If no stopping criteria, add to streamline
    points.push_back(StreamlineTracing::VoxelToObjectSpace(currentPosition, dims, spacing));

    // Sample principal direction
    const glm::vec3 direction = SamplePrincipalDirection(
        currentPosition,
        dims,
        spacing,
        evx,
        evy,
        evz,
        previousDirection);

    if (glm::length(direction) <= 1e-6f)
    {
      break;
    }

    // Step towards principal direction
    const glm::vec3 nextPosition = currentPosition + direction * settings.stepSizeVoxels;
    if (glm::length(nextPosition - currentPosition) <= 1e-6f)
    {
      break;
    }

    if (step == 0)
    {
      firstStepDirection = direction;
    }
    previousDirection = direction;
    currentPosition = nextPosition;
  }

  return points;
}

/**
 * @brief Run streamline tracing for all seeds and construct the streamline mesh.
 *
 * @param context Preprocessing context containing output channels.
 * @param settings Tractography settings.
 * @param seedsVoxelSpace Seed positions in voxel coordinates (from SelectSeedPoints).
 * @return Streamline mesh, or nullptr if tracing produced no valid streamlines.
 */
std::shared_ptr<Mesh> DtiFiberTractographyStage::BuildStreamlineMesh(const MriPreprocessingContext &context,
                                                                     const TractographySettings &settings,
                                                                     const std::vector<glm::vec3> &seedsVoxelSpace)
{
  const VolumeData &faVolume = context.outputDtiChannels.FA;
  const VolumeData &l1Volume = context.outputDtiChannels.L1;
  const VolumeData &evxVolume = context.outputDtiChannels.EVx;
  const VolumeData &evyVolume = context.outputDtiChannels.EVy;
  const VolumeData &evzVolume = context.outputDtiChannels.EVz;

  const VolumeMetadata &metadata = faVolume.GetMetadata();
  const glm::ivec3 dims = metadata.dimensions;
  const glm::vec3 spacing = metadata.spacing;

  const std::vector<float> &faVoxels = faVolume.GetVoxels();
  const std::vector<float> &l1Voxels = l1Volume.GetVoxels();
  const std::vector<float> &evx = evxVolume.GetVoxels();
  const std::vector<float> &evy = evyVolume.GetVoxels();
  const std::vector<float> &evz = evzVolume.GetVoxels();

  const std::vector<glm::vec3> &seeds = seedsVoxelSpace;
  std::vector<std::vector<glm::vec3>> streamlines;
  streamlines.reserve(seeds.size());

  // Trace streamlines from each seed point.
  constexpr size_t kSeedsPerProgressCheck = 64;
  size_t tracedSeedCount = 0;
  for (const glm::vec3 &seed : seeds)
  {
    if (tracedSeedCount % kSeedsPerProgressCheck == 0)
    {
      context.ThrowIfCancelled();
      context.ReportStageFraction(
          kSeedSelectionFractionShare +
          (1.0f - kSeedSelectionFractionShare) * static_cast<float>(tracedSeedCount) / static_cast<float>(seeds.size()));
    }
    ++tracedSeedCount;

    // Trace both ways from the seed: forward along the sampled eigenvector sign, then backward
    // starting against the first forward step so the sign-continuity check walks the other way.
    glm::vec3 firstForwardDirection(0.0f);
    const std::vector<glm::vec3> forward = TraceStreamlineFromSeed(
        seed,
        dims,
        spacing,
        faVoxels,
        l1Voxels,
        evx,
        evy,
        evz,
        settings,
        glm::vec3(0.0f),
        firstForwardDirection);

    std::vector<glm::vec3> backward;
    if (glm::length(firstForwardDirection) > 1e-6f)
    {
      glm::vec3 firstBackwardDirection(0.0f);
      backward = TraceStreamlineFromSeed(
          seed,
          dims,
          spacing,
          faVoxels,
          l1Voxels,
          evx,
          evy,
          evz,
          settings,
          -firstForwardDirection,
          firstBackwardDirection);
    }

    std::vector<glm::vec3> streamline = StreamlineTracing::JoinBidirectional(backward, forward);

    // Filter out short streamlines that are unlikely to be meaningful and would add noise to the visualization.
    if (streamline.size() < settings.minPointsPerStreamline)
    {
      continue;
    }

    streamlines.push_back(std::move(streamline));
  }

  EngineLog::Message("DTI tractography: " + std::to_string(streamlines.size()) + " streamlines from " +
                     std::to_string(seeds.size()) + " seeds", "DtiTractography");

  return StreamlineTracing::BuildStreamlineTubeMesh(streamlines, settings.tubeRadius, settings.tubeRadialSegments);
}

/**
 * @brief Get the name of the preprocessing stage.
 * 
 * @return const char* 
 */
const char *DtiFiberTractographyStage::Name() const
{
  return "DTI fiber tractography";
}

/**
 * @brief Construct a streamline mesh from the DTI volume channels in the preprocessing context, and assign it to the context's output.
 * 
 * @param context 
 */
void DtiFiberTractographyStage::Execute(MriPreprocessingContext &context) const
{
  const size_t faVoxelCount = context.outputDtiChannels.FA.GetVoxelCount();
  const size_t l1VoxelCount = context.outputDtiChannels.L1.GetVoxelCount();
  const size_t evxVoxelCount = context.outputDtiChannels.EVx.GetVoxelCount();
  const size_t evyVoxelCount = context.outputDtiChannels.EVy.GetVoxelCount();
  const size_t evzVoxelCount = context.outputDtiChannels.EVz.GetVoxelCount();

  if (faVoxelCount == 0 || l1VoxelCount == 0 || evxVoxelCount == 0 ||
      evyVoxelCount == 0 || evzVoxelCount == 0)
  {
    context.report.warnings.push_back("Fiber tractography stage skipped: required FA/L1/EV channels are unavailable.");
    context.outputStreamlineMesh = nullptr;
    context.tractographySeedsVoxelSpace.clear();
    return;
  }

  if (faVoxelCount != l1VoxelCount || faVoxelCount != evxVoxelCount ||
      faVoxelCount != evyVoxelCount || faVoxelCount != evzVoxelCount)
  {
    context.report.warnings.push_back("Fiber tractography stage skipped: channel size mismatch.");
    context.outputStreamlineMesh = nullptr;
    context.tractographySeedsVoxelSpace.clear();
    return;
  }

  const TractographySettings settings = BuildTractographySettings(context);
  const std::vector<glm::vec3> seedsVoxelSpace =
      SelectSeedPoints(context, context.outputDtiChannels.FA, context.outputDtiChannels.L1, settings);

  // Seed positions are exposed for visualization independently of whether a streamline was
  // successfully traced from each one, so convert and store them regardless of the tracing
  // outcome below.
  const VolumeMetadata &metadata = context.outputDtiChannels.FA.GetMetadata();
  context.outputSeedPoints.clear();
  context.outputSeedPoints.reserve(seedsVoxelSpace.size());
  context.tractographySeedsVoxelSpace = seedsVoxelSpace;
  for (const glm::vec3 &seedVoxel : seedsVoxelSpace)
  {
    context.outputSeedPoints.push_back(
        StreamlineTracing::VoxelToObjectSpace(seedVoxel, metadata.dimensions, metadata.spacing));
  }

  const std::shared_ptr<Mesh> streamlineMesh = BuildStreamlineMesh(context, settings, seedsVoxelSpace);
  if (!streamlineMesh)
  {
    context.report.warnings.push_back("Fiber tractography stage produced no streamlines.");
    context.outputStreamlineMesh = nullptr;
    return;
  }

  context.outputStreamlineMesh = streamlineMesh;
}

/**
 * @brief Create a Dti Fiber Tractography Stage object
 *
 * @return std::unique_ptr<IMriPreprocessingStage>
 */
std::unique_ptr<IMriPreprocessingStage> CreateDtiFiberTractographyStage()
{
  return std::make_unique<DtiFiberTractographyStage>();
}
