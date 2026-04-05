#include "DTIVolume.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "Texture3D.h"

namespace
{
  constexpr float kSpacingEpsilon = 1e-4f;

  glm::vec3 NormalizeSafe(const glm::vec3 &v)
  {
    float len = glm::length(v);
    if (len > 1e-8f)
      return v / len;
    return glm::vec3(0.0f);
  }

  bool NearlyEqual(float a, float b)
  {
    return std::fabs(a - b) <= kSpacingEpsilon;
  }

  bool HasFiniteVoxelData(const VolumeData &volumeData)
  {
    const std::vector<float> &voxels = volumeData.GetVoxels();
    return std::all_of(voxels.begin(), voxels.end(),
                       [](float v)
                       {
                         return std::isfinite(v) != 0;
                       });
  }

  bool HasCompatibleMetadata(const VolumeData &candidate,
                             const VolumeMetadata &reference)
  {
    const VolumeMetadata &metadata = candidate.GetMetadata();
    return metadata.dimensions == reference.dimensions &&
           NearlyEqual(metadata.spacing.x, reference.spacing.x) &&
           NearlyEqual(metadata.spacing.y, reference.spacing.y) &&
           NearlyEqual(metadata.spacing.z, reference.spacing.z);
  }
}

/**
 * @brief Construct a new DTIVolume object from the provided DTI channels and shader.
 *    The constructor validates the input channels for metadata consistency and finite voxel values,
 *    and uploads available metrics to the GPU as textures.
 *
 * @param channels
 * @param shader
 */
DTIVolume::DTIVolume(DTIVolumeChannels channels,
                     std::shared_ptr<Shader> shader)
    : Volume(channels.Dxx.GetMetadata(), std::move(shader)),
      channels(std::move(channels))
{
  // Hallod ez konkrétan majdnem 2 héting volt egy bug amit nem találtam meg és lófaszt sem mutatott a volume render.
  // Úgy töltöttem fel a textúrát, hogy channels.Dxx
  // De a kontruktor ugye azt a pointer std::move-val kinullolta kb, és utána a this->channels-ben volt. 
  // c:
  const DTIVolumeChannels &gpuChannels = this->channels;

  // Validate metadata consistency across channels
  if (!HasCompatibleMetadata(gpuChannels.Dyy, gpuChannels.Dxx.GetMetadata()) ||
      !HasCompatibleMetadata(gpuChannels.Dzz, gpuChannels.Dxx.GetMetadata()) ||
      !HasCompatibleMetadata(gpuChannels.Dxy, gpuChannels.Dxx.GetMetadata()) ||
      !HasCompatibleMetadata(gpuChannels.Dxz, gpuChannels.Dxx.GetMetadata()) ||
      !HasCompatibleMetadata(gpuChannels.Dyz, gpuChannels.Dxx.GetMetadata()))
  {
    throw std::invalid_argument("All DTI channels must have the same dimensions and spacing.");
  }

  // Validate finite voxel data
  if (!HasFiniteVoxelData(gpuChannels.Dxx) ||
      !HasFiniteVoxelData(gpuChannels.Dyy) ||
      !HasFiniteVoxelData(gpuChannels.Dzz) ||
      !HasFiniteVoxelData(gpuChannels.Dxy) ||
      !HasFiniteVoxelData(gpuChannels.Dxz) ||
      !HasFiniteVoxelData(gpuChannels.Dyz))
  {
    throw std::invalid_argument("All DTI channels must have finite voxel values.");
  }

  // Create textures 
  textureSet.AddTexture(std::make_shared<Texture3D>(
    gpuChannels.Dxx.GetMetadata().dimensions.x,
    gpuChannels.Dxx.GetMetadata().dimensions.y,
    gpuChannels.Dxx.GetMetadata().dimensions.z,
    GL_R32F,
    GL_RED,
    GL_FLOAT,
    gpuChannels.Dxx.GetVoxels().data(),
    true));
  textureSet.AddTexture(std::make_shared<Texture3D>(
    gpuChannels.Dyy.GetMetadata().dimensions.x,
    gpuChannels.Dyy.GetMetadata().dimensions.y,
    gpuChannels.Dyy.GetMetadata().dimensions.z,
    GL_R32F,
    GL_RED,
    GL_FLOAT,
    gpuChannels.Dyy.GetVoxels().data(),
    true));
  textureSet.AddTexture(std::make_shared<Texture3D>(
    gpuChannels.Dzz.GetMetadata().dimensions.x,
    gpuChannels.Dzz.GetMetadata().dimensions.y,
    gpuChannels.Dzz.GetMetadata().dimensions.z,
    GL_R32F,
    GL_RED,
    GL_FLOAT,
    gpuChannels.Dzz.GetVoxels().data(),
    true));
  textureSet.AddTexture(std::make_shared<Texture3D>(
    gpuChannels.Dxy.GetMetadata().dimensions.x,
    gpuChannels.Dxy.GetMetadata().dimensions.y,
    gpuChannels.Dxy.GetMetadata().dimensions.z,
    GL_R32F,
    GL_RED,
    GL_FLOAT,
    gpuChannels.Dxy.GetVoxels().data(),
    true));
  textureSet.AddTexture(std::make_shared<Texture3D>(
    gpuChannels.Dxz.GetMetadata().dimensions.x,
    gpuChannels.Dxz.GetMetadata().dimensions.y,
    gpuChannels.Dxz.GetMetadata().dimensions.z,
    GL_R32F,
    GL_RED,
    GL_FLOAT,
    gpuChannels.Dxz.GetVoxels().data(),
    true));
  textureSet.AddTexture(std::make_shared<Texture3D>(
    gpuChannels.Dyz.GetMetadata().dimensions.x,
    gpuChannels.Dyz.GetMetadata().dimensions.y,
    gpuChannels.Dyz.GetMetadata().dimensions.z,
    GL_R32F,
    GL_RED,
    GL_FLOAT,
    gpuChannels.Dyz.GetVoxels().data(),
    true));
}

/**
 * @brief Uniformprovider implementation to bind DTI-specific uniforms to the shader before drawing.
 *
 * @param shader
 */
void DTIVolume::Apply(Shader &shader) const
{
  Volume::Apply(shader);
}

/**
 * @brief Draw the DTI volume using the provided frame uniforms. This sets up blending and depth state for proper volume rendering.
 *        Bind the DTI textures and draw the geometry. Restore GL state afterwards.
 * 
 * @param frameUniforms 
 */
void DTIVolume::Draw(const UniformProvider &frameUniforms) const
{
  if (!IsValid() || !visible)
  {
    return;
  }

  GLboolean previousBlendEnabled = glIsEnabled(GL_BLEND);
  GLboolean previousDepthWriteMask = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthWriteMask);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);

  shader->Use();
  frameUniforms.Apply(*shader);
  shader->Apply(*shader);
  Apply(*shader);
  shader->SetMat4("volumeObject.modelMatrix", BuildModelMatrix());
  shader->SetMat4("volumeObject.inverseModelMatrix", glm::inverse(BuildModelMatrix()));
  GetTextureSet().Bind(*shader, "volumeTextures");
  
  geometry->Draw(*shader);

  if (!previousBlendEnabled)
  {
    glDisable(GL_BLEND);
  }
  glDepthMask(previousDepthWriteMask);
}

/**
 * @brief IInspectable implementation. Add the DTI-specific fields to the inspectable fields for UI editing.
 *
 * @param out
 * @param groupPrefix
 */
void DTIVolume::CollectInspectableFields(std::vector<UiField> &out, const std::string &groupPrefix)
{
  Volume::CollectInspectableFields(out, groupPrefix);

  const std::string group = groupPrefix.empty() ? "DTI" : (groupPrefix + "/DTI");
}

/**
 * @brief Return the major eigenvector at the given voxel coordinate.
 *
 * @param voxelCoord
 * @param outVector
 */
void DTIVolume::GetMajorEigenVectorAt(glm::ivec3 voxelCoord, glm::vec3 &outVector) const
{
  // 1. Fetch tensor components at voxel
  float Dxx = channels.Dxx.GetValue(voxelCoord);
  float Dyy = channels.Dyy.GetValue(voxelCoord);
  float Dzz = channels.Dzz.GetValue(voxelCoord);
  float Dxy = channels.Dxy.GetValue(voxelCoord);
  float Dxz = channels.Dxz.GetValue(voxelCoord);
  float Dyz = channels.Dyz.GetValue(voxelCoord);

  // 2. Construct symmetric tensor
  // [ Dxx Dxy Dxz ]
  // [ Dxy Dyy Dyz ]
  // [ Dxz Dyz Dzz ]

  // 3. Power iteration to find dominant eigenvector
  glm::vec3 v(1.0f, 0.0f, 0.0f); // initial guess
  v = NormalizeSafe(v);

  const int maxIter = 50;
  const float tol = 1e-5f;

  for (int i = 0; i < maxIter; ++i)
  {
    glm::vec3 v_new;

    // Matrix-vector multiplication
    v_new.x = Dxx * v.x + Dxy * v.y + Dxz * v.z;
    v_new.y = Dxy * v.x + Dyy * v.y + Dyz * v.z;
    v_new.z = Dxz * v.x + Dyz * v.y + Dzz * v.z;

    v_new = NormalizeSafe(v_new);

    // Convergence check
    if (glm::length(v_new - v) < tol)
      break;

    v = v_new;
  }

  outVector = NormalizeSafe(v);
}
