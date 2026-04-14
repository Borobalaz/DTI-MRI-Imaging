#include "Preprocessing/stages/DWIBrainSurfaceMeshStage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "ImportedGeometry.h"
#include "VolumeData.h"

namespace
{
  struct MeshBuildBuffers
  {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::unordered_map<std::uint64_t, unsigned int> edgeToVertexIndex;
  };

  /**
   * @brief Marching tetrahedra lookup tables for:
   *        - the 8 cube corners, 
   *        - the 6 tetrahedra that subdivide a cube, 
   *        - the edges of a tetrahedron, 
   *        - and the triangulation cases for each possible inside/outside configuration of the tetrahedron corners.
   * 
   */
  constexpr std::array<glm::ivec3, 8> kCubeCornerOffsets = {
      glm::ivec3(0, 0, 0),
      glm::ivec3(1, 0, 0),
      glm::ivec3(1, 1, 0),
      glm::ivec3(0, 1, 0),
      glm::ivec3(0, 0, 1),
      glm::ivec3(1, 0, 1),
      glm::ivec3(1, 1, 1),
      glm::ivec3(0, 1, 1)};

  constexpr std::array<std::array<int, 4>, 6> kCubeTetrahedra = {{
      {{0, 5, 1, 6}},
      {{0, 1, 2, 6}},
      {{0, 2, 3, 6}},
      {{0, 3, 7, 6}},
      {{0, 7, 4, 6}},
      {{0, 4, 5, 6}},
  }};

  constexpr std::array<std::array<int, 2>, 6> kTetraEdges = {{
      {{0, 1}},
      {{1, 2}},
      {{2, 0}},
      {{0, 3}},
      {{1, 3}},
      {{2, 3}},
  }};

  constexpr std::array<std::array<int, 7>, 16> kTetraTriTable = {{
      {{-1, -1, -1, -1, -1, -1, -1}},
      {{0, 3, 2, -1, -1, -1, -1}},
      {{0, 1, 4, -1, -1, -1, -1}},
      {{1, 4, 2, 2, 4, 3, -1}},
      {{1, 2, 5, -1, -1, -1, -1}},
      {{0, 3, 5, 0, 5, 1, -1}},
      {{0, 2, 5, 0, 5, 4, -1}},
      {{5, 4, 3, -1, -1, -1, -1}},
      {{3, 4, 5, -1, -1, -1, -1}},
      {{4, 5, 0, 5, 2, 0, -1}},
      {{1, 5, 0, 5, 3, 0, -1}},
      {{5, 2, 1, -1, -1, -1, -1}},
      {{3, 4, 2, 2, 4, 1, -1}},
      {{4, 1, 0, -1, -1, -1, -1}},
      {{2, 3, 0, -1, -1, -1, -1}},
      {{-1, -1, -1, -1, -1, -1, -1}},
  }};

  /**
   * @brief Convert voxel indices to normalized object space coordinates in the range [-0.5, 0.5], 
   *        accounting for volume dimensions and spacing.
   * 
   * @param x 
   * @param y 
   * @param z 
   * @param dims 
   * @param spacing 
   * @return glm::vec3 
   */
  glm::vec3 VoxelToObjectSpace(int x,
                               int y,
                               int z,
                               const glm::ivec3 &dims,
                               const glm::vec3 &spacing)
  {
    const glm::vec3 indexPos(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    const glm::vec3 physicalPos = indexPos * spacing;

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
   * @brief Build a unique key for an edge defined by two voxel indices.
   * 
   * @param voxelA 
   * @param voxelB 
   * @return std::uint64_t 
   */
  std::uint64_t BuildEdgeKey(size_t voxelA, size_t voxelB)
  {
    const std::uint64_t minV = static_cast<std::uint64_t>(std::min(voxelA, voxelB));
    const std::uint64_t maxV = static_cast<std::uint64_t>(std::max(voxelA, voxelB));
    return (minV << 32U) | maxV;
  }

  /**
   * @brief Get the Or Create Edge Vertex object
   * 
   * @param base 
   * @param dims 
   * @param spacing 
   * @param isoValue 
   * @param cubeValues 
   * @param cubeVoxelIndices 
   * @param cornerA 
   * @param cornerB 
   * @param buffers 
   * @return unsigned int 
   */
  unsigned int GetOrCreateEdgeVertex(const glm::ivec3 &base,
                                     const glm::ivec3 &dims,
                                     const glm::vec3 &spacing,
                                     float isoValue,
                                     const std::array<float, 8> &cubeValues,
                                     const std::array<size_t, 8> &cubeVoxelIndices,
                                     int cornerA,
                                     int cornerB,
                                     MeshBuildBuffers &buffers)
  {
    const std::uint64_t edgeKey = BuildEdgeKey(cubeVoxelIndices[cornerA], cubeVoxelIndices[cornerB]);
    auto found = buffers.edgeToVertexIndex.find(edgeKey);
    if (found != buffers.edgeToVertexIndex.end())
    {
      return found->second;
    }

    const glm::ivec3 cornerOffsetA = kCubeCornerOffsets[static_cast<size_t>(cornerA)];
    const glm::ivec3 cornerOffsetB = kCubeCornerOffsets[static_cast<size_t>(cornerB)];

    const float valueA = cubeValues[static_cast<size_t>(cornerA)];
    const float valueB = cubeValues[static_cast<size_t>(cornerB)];

    float t = 0.5f;
    const float denom = valueB - valueA;
    if (std::fabs(denom) > 1e-6f)
    {
      t = std::clamp((isoValue - valueA) / denom, 0.0f, 1.0f);
    }

    const glm::vec3 posA = VoxelToObjectSpace(base.x + cornerOffsetA.x,
                                              base.y + cornerOffsetA.y,
                                              base.z + cornerOffsetA.z,
                                              dims,
                                              spacing);
    const glm::vec3 posB = VoxelToObjectSpace(base.x + cornerOffsetB.x,
                                              base.y + cornerOffsetB.y,
                                              base.z + cornerOffsetB.z,
                                              dims,
                                              spacing);

    Vertex vertex{};
    vertex.position = glm::mix(posA, posB, t);
    vertex.normal = glm::vec3(0.0f);
    vertex.texCoord = glm::vec2(0.0f);

    const unsigned int newIndex = static_cast<unsigned int>(buffers.vertices.size());
    buffers.vertices.push_back(vertex);
    buffers.edgeToVertexIndex[edgeKey] = newIndex;
    return newIndex;
  }

  /**
   * @brief Create an adjacency list mapping each vertex index to a list of neighboring vertex indices based on the triangle indices.
   * 
   * @param indices 
   * @param vertexCount 
   * @param adjacency 
   */
  void BuildVertexAdjacency(const std::vector<unsigned int> &indices,
                            size_t vertexCount,
                            std::vector<std::vector<unsigned int>> &adjacency)
  {
    adjacency.assign(vertexCount, {});
    std::vector<std::unordered_set<unsigned int>> adjacencySets(vertexCount);

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
      const unsigned int i0 = indices[i + 0];
      const unsigned int i1 = indices[i + 1];
      const unsigned int i2 = indices[i + 2];

      adjacencySets[i0].insert(i1);
      adjacencySets[i0].insert(i2);
      adjacencySets[i1].insert(i0);
      adjacencySets[i1].insert(i2);
      adjacencySets[i2].insert(i0);
      adjacencySets[i2].insert(i1);
    }

    for (size_t i = 0; i < vertexCount; ++i)
    {
      adjacency[i].reserve(adjacencySets[i].size());
      for (unsigned int n : adjacencySets[i])
      {
        adjacency[i].push_back(n);
      }
    }
  }

  /**
   * @brief Update vertex positions by moving them towards the average position of their neighbors, scaled by the given factor.
   * 
   * @param vertices 
   * @param adjacency 
   * @param factor 
   */
  void ApplyLaplacianPass(std::vector<Vertex> &vertices,
                          const std::vector<std::vector<unsigned int>> &adjacency,
                          float factor)
  {
    std::vector<glm::vec3> updatedPositions(vertices.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < vertices.size(); ++i)
    {
      const std::vector<unsigned int> &neighbors = adjacency[i];
      if (neighbors.empty())
      {
        updatedPositions[i] = vertices[i].position;
        continue;
      }

      glm::vec3 avg(0.0f);
      for (unsigned int neighborIdx : neighbors)
      {
        avg += vertices[neighborIdx].position;
      }
      avg /= static_cast<float>(neighbors.size());

      updatedPositions[i] = vertices[i].position + factor * (avg - vertices[i].position);
    }

    for (size_t i = 0; i < vertices.size(); ++i)
    {
      vertices[i].position = updatedPositions[i];
    }
  }

  /**
   * @brief Use Taubin smoothing algorithm to interpolate vertex positions and reduce noise while preserving overall shape.
   * 
   * @param vertices 
   * @param indices 
   * @param iterations 
   * @param lambda 
   * @param mu 
   */
  void ApplyTaubinSmoothing(std::vector<Vertex> &vertices,
                            const std::vector<unsigned int> &indices,
                            int iterations,
                            float lambda,
                            float mu)
  {
    if (vertices.empty() || indices.empty())
    {
      return;
    }

    std::vector<std::vector<unsigned int>> adjacency;
    BuildVertexAdjacency(indices, vertices.size(), adjacency);

    const int iterCount = std::max(iterations, 0);
    for (int iter = 0; iter < iterCount; ++iter)
    {
      ApplyLaplacianPass(vertices, adjacency, lambda);
      ApplyLaplacianPass(vertices, adjacency, mu);
    }
  }

  /**
   * @brief Compute vertex normals based on the adjacent faces' orientation.
   * 
   * @param vertices 
   * @param indices 
   */
  void ComputeVertexNormals(std::vector<Vertex> &vertices, const std::vector<unsigned int> &indices)
  {
    for (Vertex &vertex : vertices)
    {
      vertex.normal = glm::vec3(0.0f);
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
      const unsigned int i0 = indices[i + 0];
      const unsigned int i1 = indices[i + 1];
      const unsigned int i2 = indices[i + 2];

      const glm::vec3 &p0 = vertices[i0].position;
      const glm::vec3 &p1 = vertices[i1].position;
      const glm::vec3 &p2 = vertices[i2].position;

      const glm::vec3 faceNormal = glm::cross(p1 - p0, p2 - p0);
      vertices[i0].normal += faceNormal;
      vertices[i1].normal += faceNormal;
      vertices[i2].normal += faceNormal;
    }

    for (Vertex &vertex : vertices)
    {
      const float len = glm::length(vertex.normal);
      vertex.normal = (len > 1e-8f) ? (vertex.normal / len) : glm::vec3(0.0f, 1.0f, 0.0f);
    }
  }

  /**
   * @brief Run the marching tetrahedra algorithm on the given volume data to extract an isosurface mesh at the specified isoValue.
   * 
   * @param source 
   * @param isoValue represents the threshold for the surface extraction. 
   * @return std::shared_ptr<Mesh> 
   */
  std::shared_ptr<Mesh> ExtractSurfaceMeshFromVolume(const VolumeData &source, float isoValue)
  {
    const VolumeMetadata &metadata = source.GetMetadata();
    const glm::ivec3 dims = metadata.dimensions;
    if (dims.x < 2 || dims.y < 2 || dims.z < 2)
    {
      return nullptr;
    }

    const std::vector<float> &voxels = source.GetVoxels();
    MeshBuildBuffers buffers;
    buffers.vertices.reserve(static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y) * 4U);
    buffers.indices.reserve(static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y) * 12U);

    // Core algorithm
    for (int z = 0; z < dims.z - 1; ++z)
    {
      for (int y = 0; y < dims.y - 1; ++y)
      {
        for (int x = 0; x < dims.x - 1; ++x)
        {
          const glm::ivec3 base(x, y, z);

          std::array<float, 8> cubeValues{};
          std::array<size_t, 8> cubeVoxelIndices{};
          for (int c = 0; c < 8; ++c)
          {
            const glm::ivec3 corner = base + kCubeCornerOffsets[static_cast<size_t>(c)];
            const size_t flat = VolumeData::FlatIndex(corner.x, corner.y, corner.z, dims.x, dims.y);
            cubeVoxelIndices[static_cast<size_t>(c)] = flat;
            cubeValues[static_cast<size_t>(c)] = voxels[flat];
          }

          for (const auto &tetra : kCubeTetrahedra)
          {
            std::array<int, 4> tetCorner = tetra;

            int caseIndex = 0;
            for (int i = 0; i < 4; ++i)
            {
              const float value = cubeValues[static_cast<size_t>(tetCorner[static_cast<size_t>(i)])];
              if (value < isoValue)
              {
                caseIndex |= (1 << i);
              }
            }

            const std::array<int, 7> &triEdges = kTetraTriTable[static_cast<size_t>(caseIndex)];
            if (triEdges[0] < 0)
            {
              continue;
            }

            for (int t = 0; t < 6; t += 3)
            {
              if (triEdges[static_cast<size_t>(t)] < 0)
              {
                break;
              }

              std::array<unsigned int, 3> triIndices{};
              for (int e = 0; e < 3; ++e)
              {
                const int localEdgeIndex = triEdges[static_cast<size_t>(t + e)];
                const std::array<int, 2> edge = kTetraEdges[static_cast<size_t>(localEdgeIndex)];
                const int cubeCornerA = tetCorner[static_cast<size_t>(edge[0])];
                const int cubeCornerB = tetCorner[static_cast<size_t>(edge[1])];

                triIndices[static_cast<size_t>(e)] = GetOrCreateEdgeVertex(
                    base,
                    dims,
                    metadata.spacing,
                    isoValue,
                    cubeValues,
                    cubeVoxelIndices,
                    cubeCornerA,
                    cubeCornerB,
                    buffers);
              }

              buffers.indices.push_back(triIndices[0]);
              buffers.indices.push_back(triIndices[1]);
              buffers.indices.push_back(triIndices[2]);
            }
          }
        }
      }
    }

    // validation
    if (buffers.vertices.empty() || buffers.indices.empty())
    {
      return nullptr;
    }

    // Surface smoothing and surface normal computation.
    ApplyTaubinSmoothing(buffers.vertices, buffers.indices, 10, 0.5f, -0.53f);
    ComputeVertexNormals(buffers.vertices, buffers.indices);

    // Output
    std::shared_ptr<ImportedGeometry> geometry = std::make_shared<ImportedGeometry>(
        std::move(buffers.vertices),
        std::move(buffers.indices));

    // Material is assigned by the scene so preprocessing stays render-policy agnostic.
    return std::make_shared<Mesh>(geometry, nullptr);
  }
}

/**
 * @brief Get the name of the preprocessing stage.
 * 
 * @return const char* 
 */
const char *DWIBrainSurfaceMeshStage::Name() const
{
  return "DWI brain surface mesh";
}

/**
 * @brief Construct a surface mesh from the DWI volume data using marching tetrahedra. 
 *        The resulting mesh is intended to represent the brain surface and can be used for rendering or as a seed for tractography. 
 *        This pipeline assumes the provided DWI is already brain-extracted, so the FA map is used directly for isosurface extraction.
 *        If FA is unavailable, no mesh will be generated and a warning is added to the report.
 * 
 * @param context 
 */
void DWIBrainSurfaceMeshStage::Execute(MriPreprocessingContext &context) const
{
  const VolumeData *sourceVolume = nullptr;
  float isoValue = 0.15f; // 

  // Validation
  if (context.outputChannels.FA.GetVoxelCount() > 0)
  {
    sourceVolume = &context.outputChannels.FA;
  }

  if (!sourceVolume)
  {
    context.report.warnings.push_back("Brain surface mesh stage skipped: FA volume not available.");
    context.outputSurfaceMesh = nullptr;
    return;
  }

  // Run marching tetrahedra to extract surface mesh from the volume data
  const std::shared_ptr<Mesh> extractedMesh = ExtractSurfaceMeshFromVolume(*sourceVolume, isoValue);
  if (!extractedMesh)
  {
    context.report.warnings.push_back("Brain surface mesh stage produced no triangles.");
    context.outputSurfaceMesh = nullptr;
    return;
  }

  // Output mesh
  context.outputSurfaceMesh = extractedMesh;
}

/**
 * @brief Create a Dwi Brain Surface Mesh Stage object
 * 
 * @return std::unique_ptr<IMriPreprocessingStage> 
 */
std::unique_ptr<IMriPreprocessingStage> CreateDwiBrainSurfaceMeshStage()
{
  return std::make_unique<DWIBrainSurfaceMeshStage>();
}
