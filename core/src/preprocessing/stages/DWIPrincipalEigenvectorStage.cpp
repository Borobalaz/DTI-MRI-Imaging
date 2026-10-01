#include "Preprocessing/stages/DWIPrincipalEigenvectorStage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>

#include "Preprocessing/MriPreprocessingStages.h"

namespace
{
  struct SymmetricTensor3x3
  {
    float xx;
    float yy;
    float zz;
    float xy;
    float xz;
    float yz;
  };

  inline glm::vec3 NormalizeSafe(const glm::vec3& v)
  {
    const float len = glm::length(v);
    if (len > 1e-8f)
    {
      return v / len;
    }
    return glm::vec3(0.0f);
  }

  inline glm::vec3 Multiply(const SymmetricTensor3x3& t, const glm::vec3& v)
  {
    return glm::vec3(
      t.xx * v.x + t.xy * v.y + t.xz * v.z,
      t.xy * v.x + t.yy * v.y + t.yz * v.z,
      t.xz * v.x + t.yz * v.y + t.zz * v.z);
  }

  inline void OrientDeterministically(glm::vec3& v)
  {
    const float ax = std::fabs(v.x);
    const float ay = std::fabs(v.y);
    const float az = std::fabs(v.z);

    if (ax >= ay && ax >= az)
    {
      if (v.x < 0.0f)
      {
        v = -v;
      }
    }
    else if (ay >= az)
    {
      if (v.y < 0.0f)
      {
        v = -v;
      }
    }
    else if (v.z < 0.0f)
    {
      v = -v;
    }
  }

  glm::vec3 PrincipalEigenvector(const SymmetricTensor3x3& tensor)
  {
    glm::vec3 v(1.0f, 0.0f, 0.0f);
    v = NormalizeSafe(v);

    constexpr int kMaxIterations = 48;
    constexpr float kTolerance = 1e-5f;

    for (int iteration = 0; iteration < kMaxIterations; ++iteration)
    {
      glm::vec3 next = NormalizeSafe(Multiply(tensor, v));
      if (glm::length(next - v) < kTolerance)
      {
        v = next;
        break;
      }
      v = next;
    }

    v = NormalizeSafe(v);
    if (glm::length(v) <= 1e-8f)
    {
      return glm::vec3(0.0f);
    }

    OrientDeterministically(v);
    return v;
  }
}

/**
 * @brief Get the name of the preprocessing stage
 * 
 * @return const char* 
 */
const char* DWIPrincipalEigenvectorStage::Name() const
{
  return "DWI principal eigenvector synthesis";
}

/**
 * @brief Compute the principal eigenvector for each voxel's diffusion tensor and store in output channels.
 * 
 * @param context 
 */
void DWIPrincipalEigenvectorStage::Execute(MriPreprocessingContext& context) const
{
  const VolumeMetadata& metadata = context.outputChannels.Dxx.GetMetadata();
  const int width = metadata.dimensions.x;
  const int height = metadata.dimensions.y;
  const int depth = metadata.dimensions.z;

  VolumeData evx(width, height, depth, metadata.spacing);
  VolumeData evy(width, height, depth, metadata.spacing);
  VolumeData evz(width, height, depth, metadata.spacing);

  const std::vector<float>& dxx = context.outputChannels.Dxx.GetVoxels();
  const std::vector<float>& dyy = context.outputChannels.Dyy.GetVoxels();
  const std::vector<float>& dzz = context.outputChannels.Dzz.GetVoxels();
  const std::vector<float>& dxy = context.outputChannels.Dxy.GetVoxels();
  const std::vector<float>& dxz = context.outputChannels.Dxz.GetVoxels();
  const std::vector<float>& dyz = context.outputChannels.Dyz.GetVoxels();

  std::vector<float>& evxOut = evx.GetVoxels();
  std::vector<float>& evyOut = evy.GetVoxels();
  std::vector<float>& evzOut = evz.GetVoxels();

  const size_t voxelCount = dxx.size();
  for (size_t i = 0; i < voxelCount; ++i)
  {
    // Null bad voxels (NaN, inf, ...)
    if (!std::isfinite(dxx[i]) || !std::isfinite(dyy[i]) || !std::isfinite(dzz[i]) ||
        !std::isfinite(dxy[i]) || !std::isfinite(dxz[i]) || !std::isfinite(dyz[i]))
    {
      evxOut[i] = 0.0f;
      evyOut[i] = 0.0f;
      evzOut[i] = 0.0f;
      continue;
    }

    const SymmetricTensor3x3 tensor{
      dxx[i], dyy[i], dzz[i],
      dxy[i], dxz[i], dyz[i]};

    // Compute principal eigenvector
    const glm::vec3 eigenvector = PrincipalEigenvector(tensor);
    evxOut[i] = eigenvector.x;
    evyOut[i] = eigenvector.y;
    evzOut[i] = eigenvector.z;
  }

  // Move outputs to context
  context.outputChannels.EVx = std::move(evx);
  context.outputChannels.EVy = std::move(evy);
  context.outputChannels.EVz = std::move(evz);
}

/**
 * @brief Create a Dwi Principal Eigenvector Stage object
 * 
 * @return std::unique_ptr<IMriPreprocessingStage> 
 */
std::unique_ptr<IMriPreprocessingStage> CreateDwiPrincipalEigenvectorStage()
{
  return std::make_unique<DWIPrincipalEigenvectorStage>();
}