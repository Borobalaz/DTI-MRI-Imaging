#include "DTIVolume.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>

#include "Texture3D.h"
#include "Scene/Scene.h"

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
DTIVolume::DTIVolume(DTIVolumeChannels channels,
                     std::shared_ptr<Shader> shader)
    : Volume(channels.Dxx.GetDimensions(), channels.Dxx.GetSpacing(), std::move(shader)),
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
 * @brief IInspectable implementation. Add the DTI-specific fields to the inspectable fields for UI editing.
 *
 * @param out
 * @param groupPrefix
 */
void DTIVolume::CollectInspectableFields(std::vector<UiField> &out, const std::string &groupPrefix)
{
  Volume::CollectInspectableFields(out, groupPrefix);

  const std::string group = groupPrefix.empty() ? "DTI" : (groupPrefix + "/DTI");

  // Render mode selection field
  UiField renderModeField;
  renderModeField.group = group;
  renderModeField.label = "Render Mode";
  renderModeField.kind = UiFieldKind::ComboBox;
  renderModeField.comboItems.reserve(renderModes.size());
  for (const RenderMode &mode : renderModes)
  {
    renderModeField.comboItems.push_back(mode.label);
  }
  renderModeField.getter = [this]() -> UiFieldValue
  {
    return selectedRenderMode;
  };
  renderModeField.setter = [this](const UiFieldValue &value)
  {
    if (!std::holds_alternative<int>(value))
    {
      return;
    }

    const int selected = std::get<int>(value);
    if (!SetActiveRenderMode(selected))
    {
      std::cout << "Failed to switch DTI render mode index: " << selected << std::endl;
    }
  };
  out.push_back(std::move(renderModeField));

  // Only show channel selection when in the channel slice render mode (index 0)
  if (selectedRenderMode == 0)
  {
    UiField selectedChannelField;
    selectedChannelField.group = group;
    selectedChannelField.label = "Selected Channel";
    selectedChannelField.kind = UiFieldKind::ComboBox;
    selectedChannelField.comboItems = {
        "Dxx", "Dyy", "Dzz", "Dxy", "Dxz", "Dyz",
        "EVx", "EVy", "EVz",
        "FA", "MD", "AD", "RD",
        "L1", "L2", "L3"};
    selectedChannelField.getter = [this]() -> UiFieldValue
    {
      return selectedChannel;
    };
    selectedChannelField.setter = [this](const UiFieldValue &value)
    {
      if (!std::holds_alternative<int>(value))
      {
        return;
      }

      const int selected = std::get<int>(value);
      if (selected < 0 || selected > 15)
      {
        return;
      }

      selectedChannel = selected;
    };
    out.push_back(std::move(selectedChannelField));
  }
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
      "shaders/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_tensor_fragment.glsl");
  if (channelSliceShader && channelSliceShader->ID != 0)
  {
    (*channelSliceShader)["shader.sliceZ"] = 0.5f;
    channelSliceShader->SetUniformUiFloatRange("shader.sliceZ", 0.0f, 1.0f, 0.001f);
    renderModes.push_back(RenderMode{"Channel Slice", channelSliceShader});
  }

  // Principal Eigenvector RGB Mode
  std::shared_ptr<Shader> principalDirectionShader = std::make_shared<Shader>(
      "shaders/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_ev_slice_fragment.glsl");

  if (principalDirectionShader && principalDirectionShader->ID != 0)
  {
    (*principalDirectionShader)["shader.sliceZ"] = 0.5f;
    principalDirectionShader->SetUniformUiFloatRange("shader.sliceZ", 0.0f, 1.0f, 0.001f);
    (*principalDirectionShader)["shader.density"] = 1.0f;
    principalDirectionShader->SetUniformUiFloatRange("shader.density", 0.0f, 20.0f, 0.01f);
    renderModes.push_back(RenderMode{"Principal EV RGB", principalDirectionShader});
  }

  // 3D FA Raymarch Mode
  std::shared_ptr<Shader> faRaymarchShader = std::make_shared<Shader>(
      "shaders/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_fa_raymarch_fragment.glsl");

  if (faRaymarchShader && faRaymarchShader->ID != 0)
  {
    (*faRaymarchShader)["shader.threshold"] = 0.15f;
    faRaymarchShader->SetUniformUiFloatRange("shader.threshold", 0.0f, 1.0f, 0.001f);
    (*faRaymarchShader)["shader.density"] = 1.0f;
    faRaymarchShader->SetUniformUiFloatRange("shader.density", 0.0f, 20.0f, 0.01f);
    (*faRaymarchShader)["shader.stepSize"] = 0.0f;
    faRaymarchShader->SetUniformUiFloatRange("shader.stepSize", 0.0f, 0.1f, 0.0005f);
    (*faRaymarchShader)["shader.maxSteps"] = 512;
    faRaymarchShader->SetUniformUiIntRange("shader.maxSteps", 1, 2048);
    renderModes.push_back(RenderMode{"FA 3D Raymarch", faRaymarchShader});
  }

  // 3D direction color-coded raymarch mode.
  std::shared_ptr<Shader> directionRaymarchShader = std::make_shared<Shader>(
      "shaders/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_direction_raymarch_fragment.glsl");

  if (directionRaymarchShader && directionRaymarchShader->ID != 0)
  {
    (*directionRaymarchShader)["shader.threshold"] = 0.15f;
    directionRaymarchShader->SetUniformUiFloatRange("shader.threshold", 0.0f, 1.0f, 0.001f);
    (*directionRaymarchShader)["shader.density"] = 1.0f;
    directionRaymarchShader->SetUniformUiFloatRange("shader.density", 0.0f, 20.0f, 0.01f);
    (*directionRaymarchShader)["shader.stepSize"] = 0.0005f;
    directionRaymarchShader->SetUniformUiFloatRange("shader.stepSize", 0.0005f, 0.1f, 0.0005f);
    (*directionRaymarchShader)["shader.maxSteps"] = 512;
    directionRaymarchShader->SetUniformUiIntRange("shader.maxSteps", 1, 2048);
    renderModes.push_back(RenderMode{"Direction 3D Raymarch", directionRaymarchShader});
  }

  // Lit FA isosurface mode with ambient occlusion.
  std::shared_ptr<Shader> surfaceLitShader = std::make_shared<Shader>(
      "shaders/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_surface_lit_fragment.glsl");

  if (surfaceLitShader && surfaceLitShader->ID != 0)
  {
    (*surfaceLitShader)["shader.threshold"] = 0.1f;
    surfaceLitShader->SetUniformUiFloatRange("shader.threshold", 0.0f, 1.0f, 0.001f);
    (*surfaceLitShader)["shader.stepSize"] = 0.0f;
    surfaceLitShader->SetUniformUiFloatRange("shader.stepSize", 0.0f, 0.1f, 0.0005f);
    (*surfaceLitShader)["shader.maxSteps"] = 200;
    surfaceLitShader->SetUniformUiIntRange("shader.maxSteps", 1, 2048);
    (*surfaceLitShader)["shader.specularStrength"] = 0.5f;
    surfaceLitShader->SetUniformUiFloatRange("shader.specularStrength", 0.0f, 2.0f, 0.01f);
    (*surfaceLitShader)["shader.shininess"] = 18.0f;
    surfaceLitShader->SetUniformUiFloatRange("shader.shininess", 2.0f, 256.0f, 1.0f);
    (*surfaceLitShader)["shader.aoStrength"] = 0.85f;
    surfaceLitShader->SetUniformUiFloatRange("shader.aoStrength", 0.0f, 2.0f, 0.01f);
    (*surfaceLitShader)["shader.aoRadius"] = 0.04f;
    surfaceLitShader->SetUniformUiFloatRange("shader.aoRadius", 0.005f, 0.2f, 0.001f);
    (*surfaceLitShader)["shader.aoSamples"] = 8;
    surfaceLitShader->SetUniformUiIntRange("shader.aoSamples", 1, 24);
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