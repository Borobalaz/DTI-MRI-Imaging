#include "Volume/DTIVolume.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>

#include "Texture/Texture3D.h"
#include "Scene/Scene.h"

#include "ui/mediator/InspectDropdownField.h"

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
           const glm::ivec3 &referenceDimensions,
           const glm::vec3 &referenceSpacing)
  {
      const glm::ivec3 &dimensions = candidate.GetDimensions();
      const glm::vec3 &spacing = candidate.GetSpacing();
      return dimensions == referenceDimensions &&
        NearlyEqual(spacing.x, referenceSpacing.x) &&
        NearlyEqual(spacing.y, referenceSpacing.y) &&
        NearlyEqual(spacing.z, referenceSpacing.z);
  }

  std::vector<float> PackRgbaChannels(const VolumeData &r,
                                      const VolumeData &g,
                                      const VolumeData &b,
                                      const VolumeData &a)
  {
    const std::vector<float> &rVoxels = r.GetVoxels();
    const std::vector<float> &gVoxels = g.GetVoxels();
    const std::vector<float> &bVoxels = b.GetVoxels();
    const std::vector<float> &aVoxels = a.GetVoxels();

    const size_t voxelCount = rVoxels.size();
    std::vector<float> packed(voxelCount * 4U, 0.0f);

    for (size_t i = 0; i < voxelCount; ++i)
    {
      const size_t base = i * 4U;
      packed[base + 0U] = rVoxels[i];
      packed[base + 1U] = gVoxels[i];
      packed[base + 2U] = bVoxels[i];
      packed[base + 3U] = aVoxels[i];
    }

    return packed;
  }

  std::vector<float> PackRgbChannels(const VolumeData &r,
                                     const VolumeData &g,
                                     const VolumeData &b)
  {
    const std::vector<float> &rVoxels = r.GetVoxels();
    const std::vector<float> &gVoxels = g.GetVoxels();
    const std::vector<float> &bVoxels = b.GetVoxels();

    const size_t voxelCount = rVoxels.size();
    std::vector<float> packed(voxelCount * 3U, 0.0f);

    for (size_t i = 0; i < voxelCount; ++i)
    {
      const size_t base = i * 3U;
      packed[base + 0U] = rVoxels[i];
      packed[base + 1U] = gVoxels[i];
      packed[base + 2U] = bVoxels[i];
    }

    return packed;
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
DTIVolume::DTIVolume(const std::string& id,
                     DTIVolumeChannels channels,
                     std::shared_ptr<Shader> shader)
    : Volume(id, channels.Dxx.GetDimensions(), channels.Dxx.GetSpacing(), std::move(shader)),
      channels(std::move(channels))
{
  // Hallod ez konkrétan majdnem 2 héting volt egy bug amit nem találtam meg és lófaszt sem mutatott a volume render.
  // Úgy töltöttem fel a textúrát, hogy channels.Dxx
  // De a kontruktor ugye azt a pointer std::move-val kinullolta kb, és utána a this->channels-ben volt.
  // c:
  const DTIVolumeChannels &gpuChannels = this->channels;

  const std::array<const VolumeData *, 16> metadataValidatedChannels = {
      &gpuChannels.Dxx,
      &gpuChannels.Dyy,
      &gpuChannels.Dzz,
      &gpuChannels.Dxy,
      &gpuChannels.Dxz,
      &gpuChannels.Dyz,
      &gpuChannels.EVx,
      &gpuChannels.EVy,
      &gpuChannels.EVz,
      &gpuChannels.FA,
      &gpuChannels.MD,
      &gpuChannels.AD,
      &gpuChannels.RD,
      &gpuChannels.L1,
      &gpuChannels.L2,
      &gpuChannels.L3};

  const glm::ivec3 &referenceDimensions = gpuChannels.Dxx.GetDimensions();
  const glm::vec3 &referenceSpacing = gpuChannels.Dxx.GetSpacing();

  // Validate metadata consistency across channels
  for (const VolumeData *channel : metadataValidatedChannels)
  {
    if (!HasCompatibleMetadata(*channel, referenceDimensions, referenceSpacing))
    {
      throw std::invalid_argument("All DTI channels must have the same dimensions and spacing.");
    }
  }

  // Validate finite voxel data
  for (const VolumeData *channel : metadataValidatedChannels)
  {
    if (!HasFiniteVoxelData(*channel))
    {
      throw std::invalid_argument("All DTI channels must have finite voxel values.");
    }
  }

  const int width = referenceDimensions.x;
  const int height = referenceDimensions.y;
  const int depth = referenceDimensions.z;

  // Texture 0 (RGB): (Dxx, Dyy, Dzz)
  const std::vector<float> tensorDiagRgb = PackRgbChannels(
      gpuChannels.Dxx,
      gpuChannels.Dyy,
      gpuChannels.Dzz);
  textureSet.AddTexture(std::make_shared<Texture3D>(
      width,
      height,
      depth,
      GL_RGB32F,
      GL_RGB,
      GL_FLOAT,
      tensorDiagRgb.data(),
      true));

  // Texture 1 (RGB): (Dxy, Dxz, Dyz)
  const std::vector<float> tensorOffDiagRgb = PackRgbChannels(
      gpuChannels.Dxy,
      gpuChannels.Dxz,
      gpuChannels.Dyz);
  textureSet.AddTexture(std::make_shared<Texture3D>(
      width,
      height,
      depth,
      GL_RGB32F,
      GL_RGB,
      GL_FLOAT,
      tensorOffDiagRgb.data(),
      true));

  // Texture 2 (RGB): (EVx, EVy, EVz)
  const std::vector<float> principalEigenvectorRgb = PackRgbChannels(
      gpuChannels.EVx,
      gpuChannels.EVy,
      gpuChannels.EVz);
  textureSet.AddTexture(std::make_shared<Texture3D>(
      width,
      height,
      depth,
      GL_RGB32F,
      GL_RGB,
      GL_FLOAT,
      principalEigenvectorRgb.data(),
      true));

  // Texture 3 (RGB): (L1, L2, L3)
  const std::vector<float> eigenvaluesRgb = PackRgbChannels(
      gpuChannels.L1,
      gpuChannels.L2,
      gpuChannels.L3);
  textureSet.AddTexture(std::make_shared<Texture3D>(
      width,
      height,
      depth,
      GL_RGB32F,
      GL_RGB,
      GL_FLOAT,
      eigenvaluesRgb.data(),
      true));

  // Texture 4: (FA, MD, AD, RD)
  const std::vector<float> scalarMetricsRgba = PackRgbaChannels(
      gpuChannels.FA,
      gpuChannels.MD,
      gpuChannels.AD,
      gpuChannels.RD);
  textureSet.AddTexture(std::make_shared<Texture3D>(
      width,
      height,
      depth,
      GL_RGBA32F,
      GL_RGBA,
      GL_FLOAT,
      scalarMetricsRgba.data(),
      true));

  InitializeRenderModes();
}

/**
 * @brief Uniformprovider implementation to bind DTI-specific uniforms to the shader before drawing.
 *
 * @param shader
 */
void DTIVolume::Apply(Shader &shader) const
{
  Volume::Apply(shader);
  shader["shader.selectedChannel"] = selectedChannel;
}

/**
 * @brief Draw the DTI volume using the provided frame uniforms. This sets up blending and depth state for proper volume rendering.
 *        Bind the DTI textures and draw the geometry. Restore GL state afterwards.
 *
 * @param frameUniforms
 */
void DTIVolume::Draw(const UniformProvider &frameUniforms) const
{
  Volume::Draw(frameUniforms);
}


/**
 * @brief Initialize the available render modes for the DTI volume.
 *        Each render mode corresponds to a different shader that visualizes the DTI data in a specific way (e.g. channel slice, principal eigenvector RGB).
 *
 */
void DTIVolume::InitializeRenderModes()
{
  renderModes.clear();

  // Channel Slice Mode
  std::shared_ptr<Shader> channelSliceShader = std::make_shared<Shader>(
      "dti_channel_slice_shader",
      "shaders/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_tensor_fragment.glsl");
  if (channelSliceShader && channelSliceShader->ID != 0)
  {
    (*channelSliceShader)["shader.sliceZ"] = sliceZValue;
    renderModes.push_back(RenderMode{"Channel Slice", channelSliceShader});
  }

  // Principal Eigenvector RGB Mode
  std::shared_ptr<Shader> principalDirectionShader = std::make_shared<Shader>(
      "dti_principal_ev_shader",
      "shaders/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_ev_slice_fragment.glsl");

  if (principalDirectionShader && principalDirectionShader->ID != 0)
  {
    (*principalDirectionShader)["shader.sliceZ"] = sliceZValue;
    (*principalDirectionShader)["shader.density"] = 1.0f;
    renderModes.push_back(RenderMode{"Principal EV RGB", principalDirectionShader});
  }

  // 3D FA Raymarch Mode
  std::shared_ptr<Shader> faRaymarchShader = std::make_shared<Shader>(
      "dti_fa_raymarch_shader",
      "shaders/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_fa_raymarch_fragment.glsl");

  if (faRaymarchShader && faRaymarchShader->ID != 0)
  {
    (*faRaymarchShader)["shader.threshold"] = 0.15f;
    (*faRaymarchShader)["shader.density"] = 1.0f;
    (*faRaymarchShader)["shader.stepSize"] = 0.0f;
    (*faRaymarchShader)["shader.maxSteps"] = 512;
    renderModes.push_back(RenderMode{"FA 3D Raymarch", faRaymarchShader});
  }

  // 3D direction color-coded raymarch mode.
  std::shared_ptr<Shader> directionRaymarchShader = std::make_shared<Shader>(
      "dti_direction_raymarch_shader",
      "shaders/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_direction_raymarch_fragment.glsl");

  if (directionRaymarchShader && directionRaymarchShader->ID != 0)
  {
    (*directionRaymarchShader)["shader.threshold"] = 0.15f;
    (*directionRaymarchShader)["shader.density"] = 1.0f;
    (*directionRaymarchShader)["shader.stepSize"] = 0.0005f;
    (*directionRaymarchShader)["shader.maxSteps"] = 512;
    renderModes.push_back(RenderMode{"Direction 3D Raymarch", directionRaymarchShader});
  }

  // Lit FA isosurface mode with ambient occlusion.
  std::shared_ptr<Shader> surfaceLitShader = std::make_shared<Shader>(
      "dti_surface_lit_shader",
      "shaders/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_surface_lit_fragment.glsl");

  if (surfaceLitShader && surfaceLitShader->ID != 0)
  {
    (*surfaceLitShader)["shader.threshold"] = 0.1f;
    (*surfaceLitShader)["shader.stepSize"] = 0.0f;
    (*surfaceLitShader)["shader.maxSteps"] = 200;
    (*surfaceLitShader)["shader.specularStrength"] = 0.5f;
    (*surfaceLitShader)["shader.shininess"] = 18.0f;
    (*surfaceLitShader)["shader.aoStrength"] = 0.85f;
    (*surfaceLitShader)["shader.aoRadius"] = 0.04f;
    (*surfaceLitShader)["shader.aoSamples"] = 8;
    renderModes.push_back(RenderMode{"FA Surface Lit", surfaceLitShader});
  }

  // Additional render modes can be added here following the same pattern.

  if (renderModes.empty())
  {
    throw std::runtime_error("DTIVolume could not initialize any valid render mode shader.");
  }

  SetActiveRenderMode(0);
}

/**
 * @brief Set the active render mode by index. This changes which shader is used for rendering the DTI volume.
 *
 * @param modeIndex
 * @return true
 * @return false
 */
bool DTIVolume::SetActiveRenderMode(int modeIndex)
{
  if (modeIndex < 0 || modeIndex >= static_cast<int>(renderModes.size()))
  {
    return false;
  }

  const RenderMode &mode = renderModes[static_cast<size_t>(modeIndex)];
  if (!mode.shader || mode.shader->ID == 0)
  {
    return false;
  }

  selectedRenderMode = modeIndex;
  shader = mode.shader; // Set the base shader to the active render mode shader. This will be used in Draw().
  return true;
}

/**
 * @brief Get a pointer to the active render mode.
 *
 * @return const DTIVolume::RenderMode*
 */
const DTIVolume::RenderMode *DTIVolume::GetActiveRenderMode() const
{
  if (selectedRenderMode < 0 || selectedRenderMode >= static_cast<int>(renderModes.size()))
  {
    return nullptr;
  }

  return &renderModes[static_cast<size_t>(selectedRenderMode)];
}

/**
 * @brief Register all render mode shaders with a scene for hot reload tracking.
 *        This allows shader changes to be detected and reloaded at runtime.
 *
 * @param scene The scene to register shaders with
 */
void DTIVolume::RegisterShadersWithScene(Scene* scene)
{
  if (!scene)
  {
    return;
  }

  for (size_t i = 0; i < renderModes.size(); ++i)
  {
    const auto& mode = renderModes[i];
    if (mode.shader)
    {
      // Use render mode label as a unique identifier for the shader
      std::string shaderId = "DTIVolume/" + mode.label;
      scene->RegisterShader(shaderId, mode.shader);
    }
  }
}

std::vector<std::shared_ptr<InspectField>> DTIVolume::GetInspectFields()
{  
  std::vector<std::shared_ptr<InspectField>> fields;

  const std::vector<std::shared_ptr<InspectField>> baseFields = Volume::GetInspectFields();
  fields.insert(fields.end(), baseFields.begin(), baseFields.end());

  QStringList renderModeOptions;
  for (const auto& mode : renderModes)
  {
    renderModeOptions << QString::fromStdString(mode.label);
  }

  fields.push_back(std::make_shared<InspectDropdownField>(
      "selectedRenderMode",
      "Render Mode",
      "Visualization",
      renderModeOptions,
      [this]() -> QString
      {
        if (selectedRenderMode < 0 || selectedRenderMode >= static_cast<int>(renderModes.size()))
        {
          return QString();
        }

        return QString::fromStdString(renderModes[static_cast<size_t>(selectedRenderMode)].label);
      },
      [this](const QString& newValue)
      {
        for (size_t i = 0; i < renderModes.size(); ++i)
        {
          if (QString::fromStdString(renderModes[i].label) == newValue)
          {
            SetActiveRenderMode(static_cast<int>(i));
            break;
          }
        }
      }
      ));

  if (selectedRenderMode == 0) // Channel Slice mode has an additional uniform for selected channel
  {
    QStringList channelOptions = {
        "Dxx", "Dyy", "Dzz", "Dxy", "Dxz", "Dyz",
        "EVx", "EVy", "EVz",
        "FA", "MD", "AD", "RD",
        "L1", "L2", "L3"
    };

    fields.push_back(std::make_shared<InspectDropdownField>(
        "selectedChannel",
        "Selected Channel",
        "Visualization",
        channelOptions,
        [this, channelOptions]() -> QString
        {
          if (selectedChannel < 0 || selectedChannel >= static_cast<int>(channelOptions.size()))
          {
            return QString();
          }

          return channelOptions[static_cast<size_t>(selectedChannel)];
        },
        [this, channelOptions](const QString& newValue)
        {
          for (size_t i = 0; i < channelOptions.size(); ++i)
          {
            if (channelOptions[static_cast<size_t>(i)] == newValue)
            {
              selectedChannel = static_cast<int>(i);
              break;
            }
          }
        }
      ));
  }

  return fields;
}
