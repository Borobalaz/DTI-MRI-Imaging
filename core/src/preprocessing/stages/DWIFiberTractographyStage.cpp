#include "Preprocessing/stages/DWIFiberTractographyStage.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "Preprocessing/MriTractographySettings.h"
#include "Preprocessing/MriPreprocessingStages.h"
#include "Geometry/TubeGeometry.h"
#include "Volume/VolumeData.h"

struct DWIFiberTractographyStage::TractographySettings
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
DWIFiberTractographyStage::TractographySettings DWIFiberTractographyStage::BuildTractographySettings(
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
 * @brief Linearly interpolate between two scalar values.
 * 
 * @param a First value.
 * @param b Second value.
 * @param t Interpolation factor in the range [0, 1].
 * @return Interpolated value.
 */
float DWIFiberTractographyStage::Lerp(float a, float b, float t)
{
  return a + (b - a) * t;
}

/**
 * @brief Sample a scalar volume using trilinear interpolation in voxel space.
 * 
 * @param voxels Flat voxel buffer.
 * @param point Sample position in voxel coordinates.
 * @param dims Volume dimensions.
 * @return Interpolated scalar value.
 */
float DWIFiberTractographyStage::SampleTrilinear(const std::vector<float> &voxels,
                                                 const glm::vec3 &point,
                                                 const glm::ivec3 &dims)
{
  if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0)
  {
    return 0.0f;
  }

  const float maxX = static_cast<float>(std::max(dims.x - 1, 0));
  const float maxY = static_cast<float>(std::max(dims.y - 1, 0));
  const float maxZ = static_cast<float>(std::max(dims.z - 1, 0));

  const float x = std::clamp(point.x, 0.0f, maxX);
  const float y = std::clamp(point.y, 0.0f, maxY);
  const float z = std::clamp(point.z, 0.0f, maxZ);

  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int z0 = static_cast<int>(std::floor(z));
  const int x1 = std::min(x0 + 1, dims.x - 1);
  const int y1 = std::min(y0 + 1, dims.y - 1);
  const int z1 = std::min(z0 + 1, dims.z - 1);

  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);
  const float tz = z - static_cast<float>(z0);

  const float c000 = voxels[VolumeData::FlatIndex(x0, y0, z0, dims.x, dims.y)];
  const float c100 = voxels[VolumeData::FlatIndex(x1, y0, z0, dims.x, dims.y)];
  const float c010 = voxels[VolumeData::FlatIndex(x0, y1, z0, dims.x, dims.y)];
  const float c110 = voxels[VolumeData::FlatIndex(x1, y1, z0, dims.x, dims.y)];
  const float c001 = voxels[VolumeData::FlatIndex(x0, y0, z1, dims.x, dims.y)];
  const float c101 = voxels[VolumeData::FlatIndex(x1, y0, z1, dims.x, dims.y)];
  const float c011 = voxels[VolumeData::FlatIndex(x0, y1, z1, dims.x, dims.y)];
  const float c111 = voxels[VolumeData::FlatIndex(x1, y1, z1, dims.x, dims.y)];

  const float c00 = Lerp(c000, c100, tx);
  const float c10 = Lerp(c010, c110, tx);
  const float c01 = Lerp(c001, c101, tx);
  const float c11 = Lerp(c011, c111, tx);
  const float c0 = Lerp(c00, c10, ty);
  const float c1 = Lerp(c01, c11, ty);
  const float value = Lerp(c0, c1, tz);
  return std::isfinite(value) ? value : 0.0f;
}

/**
 * @brief Check if a point in voxel space is within the bounds of the volume dimensions.
 * 
 * @param point Point in voxel coordinates.
 * @param dims Volume dimensions.
 * @return true if the point lies inside the volume bounds, false otherwise.
 */
bool DWIFiberTractographyStage::IsVoxelSpacePointInBounds(const glm::vec3 &point, const glm::ivec3 &dims)
{
  return point.x >= 0.0f && point.y >= 0.0f && point.z >= 0.0f &&
         point.x <= static_cast<float>(dims.x - 1) &&
         point.y <= static_cast<float>(dims.y - 1) &&
         point.z <= static_cast<float>(dims.z - 1);
}

/**
 * @brief Convert a position in voxel space to object space.
 * 
 * @param voxelPosition Position in voxel coordinates.
 * @param dims Volume dimensions.
 * @param spacing Voxel spacing.
 * @return Corresponding object-space position.
 */
glm::vec3 DWIFiberTractographyStage::VoxelToObjectSpace(const glm::vec3 &voxelPosition,
                                                        const glm::ivec3 &dims,
                                                        const glm::vec3 &spacing)
{
  const glm::vec3 physicalPos = voxelPosition * spacing;
  const glm::vec3 extent = glm::vec3(std::max(dims.x - 1, 1),
                                     std::max(dims.y - 1, 1),
                                     std::max(dims.z - 1, 1)) *
                           spacing;
  const float maxExtent = std::max(std::max(extent.x, extent.y), extent.z);
  if (maxExtent <= 1e-6f)
  {
    return glm::vec3(0.0f);
  }

  const glm::vec3 centered = physicalPos - extent * 0.5f;
  return centered / maxExtent;
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
glm::vec3 DWIFiberTractographyStage::SampleTrilinearDirection(const glm::vec3 &voxelPosition,
                                                             const glm::ivec3 &dims,
                                                             const std::vector<float> &evx,
                                                             const std::vector<float> &evy,
                                                             const std::vector<float> &evz)
{
  return glm::vec3(
      SampleTrilinear(evx, voxelPosition, dims),
      SampleTrilinear(evy, voxelPosition, dims),
      SampleTrilinear(evz, voxelPosition, dims));
}

/**
 * @brief Select seed points for streamline tracing using interpolated FA and L1 values.
 * 
 * @param faVolume FA volume.
 * @param l1Volume L1 volume.
 * @param settings Tractography settings.
 * @return Seed positions in voxel coordinates.
 */
std::vector<glm::vec3> DWIFiberTractographyStage::SelectSeedPoints(const VolumeData &faVolume,
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
    for (int y = 0; y < dims.y; y += stride)
    {
      for (int x = 0; x < dims.x; x += stride)
      {
        const glm::vec3 samplePoint(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        const float fa = SampleTrilinear(faVolume.GetVoxels(), samplePoint, dims);
        const float l1 = SampleTrilinear(l1Volume.GetVoxels(), samplePoint, dims);
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
glm::vec3 DWIFiberTractographyStage::SamplePrincipalDirection(const glm::vec3 &voxelPosition,
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

  // Normalize again in accoint to volume spacing to get correct step sizes in object space
  glm::vec3 indexSpaceDirection(
      direction.x / std::max(spacing.x, 1e-6f),
      direction.y / std::max(spacing.y, 1e-6f),
      direction.z / std::max(spacing.z, 1e-6f));

  const float indexLength = glm::length(indexSpaceDirection);
  if (indexLength <= 1e-6f)
  {
    return glm::vec3(0.0f);
  }

  return indexSpaceDirection / indexLength;
}

/**
 * @brief Trace a single streamline from a seed point.
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
 * @return Streamline points in object space coordinates.
 */
std::vector<glm::vec3> DWIFiberTractographyStage::TraceStreamlineFromSeed(const glm::vec3 &seedVoxel,
                                                                          const glm::ivec3 &dims,
                                                                          const glm::vec3 &spacing,
                                                                          const std::vector<float> &faVoxels,
                                                                          const std::vector<float> &l1Voxels,
                                                                          const std::vector<float> &evx,
                                                                          const std::vector<float> &evy,
                                                                          const std::vector<float> &evz,
                                                                          const TractographySettings &settings)
{
  std::vector<glm::vec3> points;
  points.reserve(static_cast<size_t>(settings.maxStepsPerStreamline));

  glm::vec3 currentPosition = seedVoxel;
  glm::vec3 previousDirection(0.0f);

  // Iterate until stopping criteria
  for (int step = 0; step < settings.maxStepsPerStreamline; ++step)
  {
    if (!IsVoxelSpacePointInBounds(currentPosition, dims))
    {
      break;
    }

    // Sample voxel data at current position using trilinear interpolation.
    const float fa = SampleTrilinear(faVoxels, currentPosition, dims);
    const float l1 = SampleTrilinear(l1Voxels, currentPosition, dims);

    if (!std::isfinite(fa) || !std::isfinite(l1) ||
        fa < settings.faStopThreshold || l1 < settings.l1StopThreshold)
    {
      break;
    }

    // If no stopping criteria, add to streamline
    points.push_back(VoxelToObjectSpace(currentPosition, dims, spacing));

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
 * @return Streamline mesh, or nullptr if tracing produced no valid streamlines.
 */
std::shared_ptr<Mesh> DWIFiberTractographyStage::BuildStreamlineMesh(const MriPreprocessingContext &context,
                                                                     const TractographySettings &settings)
{
  const VolumeData &faVolume = context.outputChannels.FA;
  const VolumeData &l1Volume = context.outputChannels.L1;
  const VolumeData &evxVolume = context.outputChannels.EVx;
  const VolumeData &evyVolume = context.outputChannels.EVy;
  const VolumeData &evzVolume = context.outputChannels.EVz;

  const VolumeMetadata &metadata = faVolume.GetMetadata();
  const glm::ivec3 dims = metadata.dimensions;
  const glm::vec3 spacing = metadata.spacing;

  const std::vector<float> &faVoxels = faVolume.GetVoxels();
  const std::vector<float> &l1Voxels = l1Volume.GetVoxels();
  const std::vector<float> &evx = evxVolume.GetVoxels();
  const std::vector<float> &evy = evyVolume.GetVoxels();
  const std::vector<float> &evz = evzVolume.GetVoxels();

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  const std::vector<glm::vec3> seeds = SelectSeedPoints(faVolume, l1Volume, settings);
  vertices.reserve(seeds.size() * settings.minPointsPerStreamline);
  indices.reserve(seeds.size() * settings.minPointsPerStreamline);

  // Trace streamlines from each seed point and build geometry.
  for (const glm::vec3 &seed : seeds)
  {
    const std::vector<glm::vec3> streamline = TraceStreamlineFromSeed(
        seed,
        dims,
        spacing,
        faVoxels,
        l1Voxels,
        evx,
        evy,
        evz,
        settings);

    // Filter out short streamlines that are unlikely to be meaningful and would add noise to the visualization.
    if (streamline.size() < settings.minPointsPerStreamline)
    {
      continue;
    }

    // Populate mesh vertices and indices, using adjacency restart indices to separate streamlines in the mesh.
    const unsigned int startIndex = static_cast<unsigned int>(vertices.size());
    for (size_t i = 0; i < streamline.size(); ++i)
    {
      const glm::vec3 &point = streamline[i];

      // Estimate tangent using forward difference, or backward difference at the end of the streamline.
      glm::vec3 tangent(0.0f, 1.0f, 0.0f);
      if (i + 1 < streamline.size())
      {
        tangent = streamline[i + 1] - point;
      }
      else if (i > 0)
      {
        tangent = point - streamline[i - 1];
      }

      // Normalize tangent and handle degenerate cases.
      const float tangentLength = glm::length(tangent);
      if (tangentLength > 1e-6f)
      {
        tangent /= tangentLength;
      }
      else
      {
        tangent = glm::vec3(0.0f, 1.0f, 0.0f);
      }

      // Create vertex
      Vertex v{};
      v.position = point;
      v.normal = tangent;
      v.tangent = tangent;
      v.texCoord = glm::vec2(0.0f, 0.0f);
      vertices.push_back(v);
    }

    // Add adjacency restart index after each streamline to allow rendering with a single draw call while keeping streamlines visually separated.
    for (size_t i = 0; i < streamline.size(); ++i)
    {
      indices.push_back(startIndex + static_cast<unsigned int>(i));
    }

    indices.push_back(std::numeric_limits<unsigned int>::max());
  }

  // Validation
  if (indices.empty() || vertices.empty())
  {
    return nullptr;
  }

  if (indices.back() == std::numeric_limits<unsigned int>::max())
  {
    indices.pop_back();
  }

  // Construct mesh
  std::shared_ptr<Geometry> geometry = std::make_shared<TubeGeometry>(
      std::move(vertices),
      std::move(indices),
      settings.tubeRadius,
      settings.tubeRadialSegments);

  // Material is assigned by the scene to keep preprocessing render-policy agnostic.
  return std::make_shared<Mesh>(geometry, nullptr);
}

/**
 * @brief Get the name of the preprocessing stage.
 * 
 * @return const char* 
 */
const char *DWIFiberTractographyStage::Name() const
{
  return "DWI fiber tractography";
}

/**
 * @brief Construct a streamline mesh from the DTI volume channels in the preprocessing context, and assign it to the context's output.
 * 
 * @param context 
 */
void DWIFiberTractographyStage::Execute(MriPreprocessingContext &context) const
{
  const size_t faVoxelCount = context.outputChannels.FA.GetVoxelCount();
  const size_t l1VoxelCount = context.outputChannels.L1.GetVoxelCount();
  const size_t evxVoxelCount = context.outputChannels.EVx.GetVoxelCount();
  const size_t evyVoxelCount = context.outputChannels.EVy.GetVoxelCount();
  const size_t evzVoxelCount = context.outputChannels.EVz.GetVoxelCount();

  if (faVoxelCount == 0 || l1VoxelCount == 0 || evxVoxelCount == 0 ||
      evyVoxelCount == 0 || evzVoxelCount == 0)
  {
    context.report.warnings.push_back("Fiber tractography stage skipped: required FA/L1/EV channels are unavailable.");
    context.outputStreamlineMesh = nullptr;
    return;
  }

  if (faVoxelCount != l1VoxelCount || faVoxelCount != evxVoxelCount ||
      faVoxelCount != evyVoxelCount || faVoxelCount != evzVoxelCount)
  {
    context.report.warnings.push_back("Fiber tractography stage skipped: channel size mismatch.");
    context.outputStreamlineMesh = nullptr;
    return;
  }

  const TractographySettings settings = BuildTractographySettings(context);
  const std::shared_ptr<Mesh> streamlineMesh = BuildStreamlineMesh(context, settings);
  if (!streamlineMesh)
  {
    context.report.warnings.push_back("Fiber tractography stage produced no streamlines.");
    context.outputStreamlineMesh = nullptr;
    return;
  }

  context.outputStreamlineMesh = streamlineMesh;
}

/**
 * @brief Create a Dwi Fiber Tractography Stage object
 * 
 * @return std::unique_ptr<IMriPreprocessingStage> 
 */
std::unique_ptr<IMriPreprocessingStage> CreateDwiFiberTractographyStage()
{
  return std::make_unique<DWIFiberTractographyStage>();
}
