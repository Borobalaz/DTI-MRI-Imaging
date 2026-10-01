#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

class DWIFiberTractographyStage final : public IMriPreprocessingStage
{
public:
  const char *Name() const override;
  void Execute(MriPreprocessingContext &context) const override;

private:
  struct TractographySettings;

  static float Lerp(float a, float b, float t);
  static float SampleTrilinear(const std::vector<float> &voxels,
                               const glm::vec3 &point,
                               const glm::ivec3 &dims);
  static TractographySettings BuildTractographySettings(const MriPreprocessingContext &context);
  static bool IsVoxelSpacePointInBounds(const glm::vec3 &point, const glm::ivec3 &dims);
  static glm::vec3 VoxelToObjectSpace(const glm::vec3 &voxelPosition,
                                      const glm::ivec3 &dims,
                                      const glm::vec3 &spacing);
  static glm::vec3 SampleTrilinearDirection(const glm::vec3 &voxelPosition,
                                            const glm::ivec3 &dims,
                                            const std::vector<float> &evx,
                                            const std::vector<float> &evy,
                                            const std::vector<float> &evz);
  static std::vector<glm::vec3> SelectSeedPoints(const VolumeData &faVolume,
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
                                                        const TractographySettings &settings);
  static std::shared_ptr<Mesh> BuildStreamlineMesh(const MriPreprocessingContext &context,
                                                   const TractographySettings &settings);
};

std::unique_ptr<IMriPreprocessingStage> CreateDwiFiberTractographyStage();
