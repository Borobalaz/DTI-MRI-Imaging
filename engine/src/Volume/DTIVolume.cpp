#include "Volume/DTIVolume.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>
#include <utility>

#include "foundation/log/EngineLog.h"
#include "render/texture/Texture3D.h"

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

  TextureData3D MakeTextureData(const glm::ivec3 &dimensions, TextureFormat format, std::vector<float> texels)
  {
    TextureData3D textureData;
    textureData.dimensions = dimensions;
    textureData.format = format;
    textureData.texels = std::move(texels);
    return textureData;
  }
}

/**
 * @brief Validate the DTI channels and pack them into the five CPU texture payloads of the
 *        DTIVolume shader contract. Pure CPU, so it runs on the preprocessing worker.
 *
 * @param channels
 * @param cancellation checked before validation and before each packed texture
 */
DTIVolumeTextureData PackDTIVolumeTextures(const DTIVolumeChannels &channels,
                                           const CancellationToken &cancellation)
{
  cancellation.ThrowIfCancellationRequested();

  const std::array<const VolumeData *, 16> metadataValidatedChannels = {
      &channels.Dxx,
      &channels.Dyy,
      &channels.Dzz,
      &channels.Dxy,
      &channels.Dxz,
      &channels.Dyz,
      &channels.EVx,
      &channels.EVy,
      &channels.EVz,
      &channels.FA,
      &channels.MD,
      &channels.AD,
      &channels.RD,
      &channels.L1,
      &channels.L2,
      &channels.L3};

  const glm::ivec3 &referenceDimensions = channels.Dxx.GetDimensions();
  const glm::vec3 &referenceSpacing = channels.Dxx.GetSpacing();

  // Validate metadata consistency across channels
  for (const VolumeData *channel : metadataValidatedChannels)
  {
    if (!HasCompatibleMetadata(*channel, referenceDimensions, referenceSpacing))
    {
      EngineLog::Error("All DTI channels must have the same dimensions and spacing.", "DTIVolume");
      throw std::invalid_argument("All DTI channels must have the same dimensions and spacing.");
    }
  }

  // Validate finite voxel data
  for (const VolumeData *channel : metadataValidatedChannels)
  {
    if (!HasFiniteVoxelData(*channel))
    {
      EngineLog::Error("All DTI channels must have finite voxel values.", "DTIVolume");
      throw std::invalid_argument("All DTI channels must have finite voxel values.");
    }
  }

  DTIVolumeTextureData packed;
  packed.dimensions = referenceDimensions;
  packed.spacing = referenceSpacing;

  // Texture 0 (RGB): (Dxx, Dyy, Dzz)
  cancellation.ThrowIfCancellationRequested();
  packed.textures[0] = MakeTextureData(referenceDimensions, TextureFormat::RGB32F,
                                       PackRgbChannels(channels.Dxx, channels.Dyy, channels.Dzz));

  // Texture 1 (RGB): (Dxy, Dxz, Dyz)
  cancellation.ThrowIfCancellationRequested();
  packed.textures[1] = MakeTextureData(referenceDimensions, TextureFormat::RGB32F,
                                       PackRgbChannels(channels.Dxy, channels.Dxz, channels.Dyz));

  // Texture 2 (RGB): (EVx, EVy, EVz)
  cancellation.ThrowIfCancellationRequested();
  packed.textures[2] = MakeTextureData(referenceDimensions, TextureFormat::RGB32F,
                                       PackRgbChannels(channels.EVx, channels.EVy, channels.EVz));

  // Texture 3 (RGB): (L1, L2, L3)
  cancellation.ThrowIfCancellationRequested();
  packed.textures[3] = MakeTextureData(referenceDimensions, TextureFormat::RGB32F,
                                       PackRgbChannels(channels.L1, channels.L2, channels.L3));

  // Texture 4 (RGBA): (FA, MD, AD, RD)
  cancellation.ThrowIfCancellationRequested();
  packed.textures[4] = MakeTextureData(referenceDimensions, TextureFormat::RGBA32F,
                                       PackRgbaChannels(channels.FA, channels.MD, channels.AD, channels.RD));

  return packed;
}

/**
 * @brief Construct a new DTIVolume from already packed textures and already built render modes.
 *        Makes no GL calls: the textures upload lazily at their first bind on the GL thread.
 *
 * @param id
 * @param textures packed by PackDTIVolumeTextures
 * @param renderModes built by CreateRenderModes; must not be empty
 */
DTIVolume::DTIVolume(const std::string& id,
                     DTIVolumeTextureData textures,
                     std::vector<RenderMode> renderModes)
    : Volume(id, textures.dimensions, textures.spacing, renderModes.empty() ? nullptr : renderModes.front().shader)
{
  if (renderModes.empty())
  {
    EngineLog::Error("DTIVolume could not initialize any valid render mode shader.", "DTIVolume");
    throw std::runtime_error("DTIVolume could not initialize any valid render mode shader.");
  }

  for (TextureData3D &textureData : textures.textures)
  {
    textureSet.AddTexture(std::make_shared<Texture3D>(std::move(textureData), true));
  }

  this->renderModes = std::move(renderModes);

  EngineLog::Message(
    "DTIVolume ready: " + std::to_string(this->renderModes.size()) + "/5 render modes available",
    "DTIVolume");

  SetActiveRenderMode(0);
}

/**
 * @brief Uniformprovider implementation to bind DTI-specific uniforms to the shader before drawing.
 *
 * @param shader
 */
void DTIVolume::Apply(IUniformSink &shader) const
{
  Volume::Apply(shader);
  shader.SetInt("shader.selectedChannel", selectedChannel);
}

/**
 * @brief Build the available render modes for a DTI volume. GL thread only (compiles shaders).
 *        Each render mode corresponds to a different shader that visualizes the DTI data in a specific way (e.g. channel slice, principal eigenvector RGB).
 *        Modes whose shader fails to build are skipped with a warning.
 *
 */
std::vector<DTIVolume::RenderMode> DTIVolume::CreateRenderModes()
{
  constexpr float kDefaultSliceZ = 0.5f;
  std::vector<RenderMode> renderModes;

  // Channel Slice Mode
  std::shared_ptr<Shader> channelSliceShader = std::make_shared<Shader>(
      "dti_channel_slice_shader",
      "shaders/volume/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_tensor_fragment.glsl");
  if (channelSliceShader && channelSliceShader->IsValid())
  {
    (*channelSliceShader)["shader.sliceZ"] = kDefaultSliceZ;
    renderModes.push_back(RenderMode{"Channel Slice", channelSliceShader});
  }
  else
  {
    EngineLog::Warning("Render mode 'Channel Slice' unavailable: shader failed to build", "DTIVolume");
  }

  // Principal Eigenvector RGB Mode
  std::shared_ptr<Shader> principalDirectionShader = std::make_shared<Shader>(
      "dti_principal_ev_shader",
      "shaders/volume/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_ev_slice_fragment.glsl");

  if (principalDirectionShader && principalDirectionShader->IsValid())
  {
    (*principalDirectionShader)["shader.sliceZ"] = kDefaultSliceZ;
    (*principalDirectionShader)["shader.density"] = 1.0f;
    renderModes.push_back(RenderMode{"Principal EV RGB", principalDirectionShader});
  }
  else
  {
    EngineLog::Warning("Render mode 'Principal EV RGB' unavailable: shader failed to build", "DTIVolume");
  }

  // 3D FA Raymarch Mode
  std::shared_ptr<Shader> faRaymarchShader = std::make_shared<Shader>(
      "dti_fa_raymarch_shader",
      "shaders/volume/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_fa_raymarch_fragment.glsl");

  if (faRaymarchShader && faRaymarchShader->IsValid())
  {
    (*faRaymarchShader)["shader.threshold"] = 0.15f;
    (*faRaymarchShader)["shader.density"] = 1.0f;
    (*faRaymarchShader)["shader.stepSize"] = 0.0f;
    (*faRaymarchShader)["shader.maxSteps"] = 512;
    renderModes.push_back(RenderMode{"FA 3D Raymarch", faRaymarchShader});
  }
  else
  {
    EngineLog::Warning("Render mode 'FA 3D Raymarch' unavailable: shader failed to build", "DTIVolume");
  }

  // 3D direction color-coded raymarch mode.
  std::shared_ptr<Shader> directionRaymarchShader = std::make_shared<Shader>(
      "dti_direction_raymarch_shader",
      "shaders/volume/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_direction_raymarch_fragment.glsl");

  if (directionRaymarchShader && directionRaymarchShader->IsValid())
  {
    (*directionRaymarchShader)["shader.threshold"] = 0.15f;
    (*directionRaymarchShader)["shader.density"] = 1.0f;
    (*directionRaymarchShader)["shader.stepSize"] = 0.0005f;
    (*directionRaymarchShader)["shader.maxSteps"] = 512;
    renderModes.push_back(RenderMode{"Direction 3D Raymarch", directionRaymarchShader});
  }
  else
  {
    EngineLog::Warning("Render mode 'Direction 3D Raymarch' unavailable: shader failed to build", "DTIVolume");
  }

  // Lit FA isosurface mode with ambient occlusion.
  std::shared_ptr<Shader> surfaceLitShader = std::make_shared<Shader>(
      "dti_surface_lit_shader",
      "shaders/volume/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_surface_lit_fragment.glsl");

  if (surfaceLitShader && surfaceLitShader->IsValid())
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
  else
  {
    EngineLog::Warning("Render mode 'FA Surface Lit' unavailable: shader failed to build", "DTIVolume");
  }

  // Additional render modes can be added here following the same pattern.

  return renderModes;
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
    EngineLog::Warning("SetActiveRenderMode: index out of range", "DTIVolume");
    return false;
  }

  const RenderMode &mode = renderModes[static_cast<size_t>(modeIndex)];
  if (!mode.shader || !mode.shader->IsValid())
  {
    EngineLog::Warning("SetActiveRenderMode: render mode '" + mode.label + "' has no built shader",
                        "DTIVolume");
    return false;
  }

  selectedRenderMode = modeIndex;
  shader = mode.shader; // Set the base shader to the active render mode shader. This will be used in Draw().
  return true;
}

bool DTIVolume::SetSelectedRenderModeIndex(int modeIndex)
{
  return SetActiveRenderMode(modeIndex);
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
 * @brief Get inspect fields for controlling DTI volume properties.
 * 
 * @return engine inspection fields
 */
std::vector<InspectFieldPtr> DTIVolume::GetInspectFields()
{  
  std::vector<InspectFieldPtr> fields;

  // Basic volume properties from the base Volume class
  const std::vector<InspectFieldPtr> baseFields = Volume::GetInspectFields();
  fields.insert(fields.end(), baseFields.begin(), baseFields.end());

  // Render mode selection
  std::vector<std::string> renderModeOptions;
  for (const auto& mode : renderModes)
  {
    renderModeOptions.push_back(mode.label);
  }

  const auto currentRenderMode = [this]() -> std::string
  {
    return selectedRenderMode >= 0 && selectedRenderMode < static_cast<int>(renderModes.size())
      ? renderModes[static_cast<size_t>(selectedRenderMode)].label
      : std::string();
  };
  auto renderModeField = MakeInspectField(
      "selectedRenderMode", "Render Mode", "Visualization", InspectFieldType::Dropdown,
      currentRenderMode(),
      [currentRenderMode]() -> InspectValue { return currentRenderMode(); },
      [this](const InspectValue& value)
      {
        const auto* newValue = std::get_if<std::string>(&value);
        if (!newValue) return;
        for (size_t i = 0; i < renderModes.size(); ++i)
        {
          if (renderModes[i].label == *newValue)
          {
            SetActiveRenderMode(static_cast<int>(i));
            break;
          }
        }
      });
  renderModeField->enumOptions = renderModeOptions;
  fields.push_back(renderModeField);

  // Dropdown for channel selection
  if (selectedRenderMode == 0) // Channel Slice mode has an additional uniform for selected channel
  {
    const std::vector<std::string> channelOptions = {
        "Dxx", "Dyy", "Dzz", "Dxy", "Dxz", "Dyz",
        "EVx", "EVy", "EVz", "FA", "MD", "AD", "RD", "L1", "L2", "L3"};
    const auto currentChannel = [this, channelOptions]() -> std::string
    {
      return selectedChannel >= 0 && selectedChannel < static_cast<int>(channelOptions.size())
        ? channelOptions[static_cast<size_t>(selectedChannel)]
        : std::string();
    };
    auto channelField = MakeInspectField(
        "selectedChannel", "Selected Channel", "Visualization", InspectFieldType::Dropdown,
        currentChannel(),
        [currentChannel]() -> InspectValue { return currentChannel(); },
        [this, channelOptions](const InspectValue& value)
        {
          const auto* newValue = std::get_if<std::string>(&value);
          if (!newValue) return;
          for (size_t i = 0; i < channelOptions.size(); ++i)
          {
            if (channelOptions[i] == *newValue)
            {
              selectedChannel = static_cast<int>(i);
              break;
            }
          }
        });
    channelField->enumOptions = channelOptions;
    fields.push_back(channelField);
  }

  return fields;
}

