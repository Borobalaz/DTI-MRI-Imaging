#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

// Deterministic, bidirectional principal-eigenvector tractography seeded at high-FA voxels. Writes
// context.outputStreamlineMesh, the object-space seeds (outputSeedPoints) and the voxel-space seeds
// (tractographySeedsVoxelSpace, reused by the fODF tracker).
class DtiFiberTractographyStage final : public IMriPreprocessingStage
{
public:
  const char *Name() const override;
  void Execute(MriPreprocessingContext &context) const override;

private:
  struct TractographySettings;

  static TractographySettings BuildTractographySettings(const MriPreprocessingContext &context);
  static glm::vec3 SampleTrilinearDirection(const glm::vec3 &voxelPosition,
                                            const glm::ivec3 &dims,
                                            const std::vector<float> &evx,
                                            const std::vector<float> &evy,
                                            const std::vector<float> &evz);
  static std::vector<glm::vec3> SelectSeedPoints(const MriPreprocessingContext &context,
                                                 const VolumeData &faVolume,
                                                 const VolumeData &l1Volume,
                                                 const TractographySettings &settings);
  static glm::vec3 SamplePrincipalDirection(const glm::vec3 &voxelPosition,
                                            const glm::ivec3 &dims,
                                            const glm::vec3 &spacing,
                                            const std::vector<float> &evx,
                                            const std::vector<float> &evy,
                                            const std::vector<float> &evz,
                                            const glm::vec3 &previousDirection);
  static std::vector<glm::vec3> TraceStreamlineFromSeed(const glm::vec3 &seedVoxel,
                                                        const glm::ivec3 &dims,
                                                        const glm::vec3 &spacing,
                                                        const std::vector<float> &faVoxels,
                                                        const std::vector<float> &l1Voxels,
                                                        const std::vector<float> &evx,
                                                        const std::vector<float> &evy,
                                                        const std::vector<float> &evz,
                                                        const TractographySettings &settings,
                                                        const glm::vec3 &initialReferenceDirection,
                                                        glm::vec3 &firstStepDirection);
  static std::shared_ptr<Mesh> BuildStreamlineMesh(const MriPreprocessingContext &context,
                                                   const TractographySettings &settings,
                                                   const std::vector<glm::vec3> &seedsVoxelSpace);
};
