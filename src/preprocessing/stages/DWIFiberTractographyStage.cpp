#include "Preprocessing/stages/DWIFiberTractographyStage.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "Preprocessing/MriPreprocessingStages.h"
#include "StreamlineGeometry.h"
#include "VolumeData.h"

namespace
{
  struct TractographySettings
  {
    float faSeedThreshold = 0.35f;
    float faStopThreshold = 0.2f;
    float l1StopThreshold = 1e-6f;
    float stepSizeVoxels = 0.75f;
    int maxStepsPerStreamline = 192;
    int seedStride = 2;
    size_t maxSeeds = 2500;
    size_t minPointsPerStreamline = 6;
  };

  /**
   * @brief Check if a point in voxel space is within the bounds of the volume dimensions.
   * 
   * @param point 
   * @param dims 
   * @return true 
   * @return false 
   */
  inline bool IsVoxelSpacePointInBounds(const glm::vec3 &point, const glm::ivec3 &dims)
  {
    return point.x >= 0.0f && point.y >= 0.0f && point.z >= 0.0f &&
           point.x <= static_cast<float>(dims.x - 1) &&
           point.y <= static_cast<float>(dims.y - 1) &&
           point.z <= static_cast<float>(dims.z - 1);
  }

  /**
   * @brief Sample the voxel at the nearest integer coordinates in voxel space.
   * 
   * @param point 
   * @param dims 
   * @return size_t 
   */
  inline size_t SampleNearestFlatIndex(const glm::vec3 &point, const glm::ivec3 &dims)
  {
    const int x = std::clamp(static_cast<int>(std::floor(point.x + 0.5f)), 0, dims.x - 1);
    const int y = std::clamp(static_cast<int>(std::floor(point.y + 0.5f)), 0, dims.y - 1);
    const int z = std::clamp(static_cast<int>(std::floor(point.z + 0.5f)), 0, dims.z - 1);
    return VolumeData::FlatIndex(x, y, z, dims.x, dims.y);
  }

  /**
   * @brief Convert a position in voxel space to object space.
   * 
   * @param voxelPosition 
   * @param dims 
   * @param spacing 
   * @return glm::vec3 
   */
  glm::vec3 VoxelToObjectSpace(const glm::vec3 &voxelPosition,
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
   * @brief Select a vector of 3d points where to start the streamline trace algorithm.
   * 
   * @param faVolume 
   * @param l1Volume 
   * @param settings 
   * @return std::vector<glm::vec3> 
   */
  std::vector<glm::vec3> SelectSeedPoints(const VolumeData &faVolume,
                                          const VolumeData &l1Volume,
                                          const TractographySettings &settings)
  {
    const VolumeMetadata &metadata = faVolume.GetMetadata();
    const glm::ivec3 dims = metadata.dimensions;
    const std::vector<float> &faVoxels = faVolume.GetVoxels();
    const std::vector<float> &l1Voxels = l1Volume.GetVoxels();

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
          // Sample FA and L1
          const size_t index = VolumeData::FlatIndex(x, y, z, dims.x, dims.y);
          const float fa = faVoxels[index];
          const float l1 = l1Voxels[index];
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
   * @brief Sample the principal eigenvector at given position in the volume
   * 
   * @param voxelPosition 
   * @param dims 
   * @param spacing 
   * @param evx 
   * @param evy 
   * @param evz 
   * @param previousDirection 
   * @return glm::vec3 
   */
  glm::vec3 SamplePrincipalDirection(const glm::vec3 &voxelPosition,
                                     const glm::ivec3 &dims,
                                     const glm::vec3 &spacing,
                                     const std::vector<float> &evx,
                                     const std::vector<float> &evy,
                                     const std::vector<float> &evz,
                                     const glm::vec3 &previousDirection)
  {
    // Get the nearest fixed voxel
    const size_t index = SampleNearestFlatIndex(voxelPosition, dims);

    glm::vec3 direction(evx[index], evy[index], evz[index]);
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
   * @param seedVoxel 
   * @param dims 
   * @param spacing 
   * @param faVoxels 
   * @param l1Voxels 
   * @param evx 
   * @param evy 
   * @param evz 
   * @param settings 
   * @return std::vector<glm::vec3> vector of points along the traced streamline in object space coordinates. May be empty if tracing failed or terminated early.
   */
  std::vector<glm::vec3> TraceStreamlineFromSeed(const glm::vec3 &seedVoxel,
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

      // Sample voxel data at current position
      const size_t index = SampleNearestFlatIndex(currentPosition, dims);
      const float fa = faVoxels[index];
      const float l1 = l1Voxels[index];

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
   * @brief Run a streamline tracing algorithm from selected points.
   *        Calculate vertex positions and tangents for each streamline, and construct a mesh with adjacency restart indices between streamlines.
   * 
   * @param context 
   * @param settings 
   * @return std::shared_ptr<Mesh> 
   */
  std::shared_ptr<Mesh> BuildStreamlineMesh(const MriPreprocessingContext &context,
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
        v.texCoord = glm::vec2(0.0f, 0.0f);
        vertices.push_back(v);
      }

      // Add adjacency restart index after each streamline to allow rendering with a single draw call while keeping streamlines visually separated.
      for (size_t i = 0; i < streamline.size(); ++i)
      {
        indices.push_back(startIndex + static_cast<unsigned int>(i));
      }

      indices.push_back(StreamlineGeometry::kRestartIndex);
    }

    // Validation
    if (indices.empty() || vertices.empty())
    {
      return nullptr;
    }

    if (indices.back() == StreamlineGeometry::kRestartIndex)
    {
      indices.pop_back();
    }

    // Construct mesh
    std::shared_ptr<Geometry> geometry = std::make_shared<StreamlineGeometry>(
        std::move(vertices),
        std::move(indices));

    // Material is assigned by the scene to keep preprocessing render-policy agnostic.
    return std::make_shared<Mesh>(geometry, nullptr);
  }
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

  const TractographySettings settings{};
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
