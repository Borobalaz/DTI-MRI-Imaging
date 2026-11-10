#include "scene/volume/FodfVolume.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "foundation/log/EngineLog.h"
#include "render/texture/Texture3D.h"
#include "scene/slicing/SlicePlane.h"

namespace
{
  constexpr const char* kLogSource = "FodfVolume";
  constexpr float kSpacingEpsilon = 1e-4f;
  constexpr float kNormalizationPercentile = 0.99f;
  // NuFO counts at most three kept peaks; the R32F texture stores NuFO / kMaxPeakCount.
  constexpr float kMaxPeakCount = 3.0f;

  constexpr const char* kChannelSliceModeLabel = "Channel Slice";
  constexpr const char* kPeakRgbSliceModeLabel = "Peak RGB Slice";

  // Order matches shader.selectedChannel in volume_fodf_channel_slice_fragment.glsl.
  const std::vector<std::string> kChannelOptions = {
      "AFD", "NuFO", "Peak 1 Amplitude", "Peak 2 Amplitude", "Peak 3 Amplitude"};

  bool NearlyEqual(float a, float b)
  {
    return std::fabs(a - b) <= kSpacingEpsilon;
  }

  bool HasCompatibleMetadata(const VolumeData& candidate,
                             const glm::ivec3& referenceDimensions,
                             const glm::vec3& referenceSpacing)
  {
    const glm::ivec3& dimensions = candidate.GetDimensions();
    const glm::vec3& spacing = candidate.GetSpacing();
    return dimensions == referenceDimensions &&
           NearlyEqual(spacing.x, referenceSpacing.x) &&
           NearlyEqual(spacing.y, referenceSpacing.y) &&
           NearlyEqual(spacing.z, referenceSpacing.z);
  }

  bool HasFiniteVoxelData(const VolumeData& volumeData)
  {
    const std::vector<float>& voxels = volumeData.GetVoxels();
    return std::all_of(voxels.begin(), voxels.end(), [](float v) { return std::isfinite(v) != 0; });
  }

  // Display scale for a raw map: the given percentile of its positive voxels, so a few outliers
  // do not crush the contrast. 1 when the map has no positive voxel (the map is all zero then).
  float PositivePercentile(const std::vector<float>& voxels, float percentile)
  {
    std::vector<float> positives;
    positives.reserve(voxels.size());
    std::copy_if(voxels.begin(), voxels.end(), std::back_inserter(positives), [](float v) { return v > 0.0f; });
    if (positives.empty())
    {
      return 1.0f;
    }

    const size_t rank = std::min(positives.size() - 1U,
                                 static_cast<size_t>(percentile * static_cast<float>(positives.size() - 1U)));
    std::nth_element(positives.begin(), positives.begin() + static_cast<std::ptrdiff_t>(rank), positives.end());
    return positives[rank];
  }

  float NormalizeToUnit(float value, float scale)
  {
    return std::clamp(value / scale, 0.0f, 1.0f);
  }

  TextureData3D MakeTextureData(const glm::ivec3& dimensions, TextureFormat format, std::vector<float> texels)
  {
    TextureData3D textureData;
    textureData.dimensions = dimensions;
    textureData.format = format;
    textureData.texels = std::move(texels);
    return textureData;
  }

  void ValidateChannels(const FodfVolumeChannels& channels)
  {
    const std::array<const VolumeData*, 14> allChannels = {
        &channels.AFD,
        &channels.NuFO,
        &channels.Peak1X,
        &channels.Peak1Y,
        &channels.Peak1Z,
        &channels.Peak2X,
        &channels.Peak2Y,
        &channels.Peak2Z,
        &channels.Peak3X,
        &channels.Peak3Y,
        &channels.Peak3Z,
        &channels.Peak1Amplitude,
        &channels.Peak2Amplitude,
        &channels.Peak3Amplitude};

    const glm::ivec3& referenceDimensions = channels.AFD.GetDimensions();
    const glm::vec3& referenceSpacing = channels.AFD.GetSpacing();
    if (channels.AFD.GetVoxelCount() == 0)
    {
      throw std::invalid_argument("fODF maps are empty.");
    }

    for (const VolumeData* channel : allChannels)
    {
      if (!HasCompatibleMetadata(*channel, referenceDimensions, referenceSpacing))
      {
        throw std::invalid_argument("All fODF map channels must have the same dimensions and spacing.");
      }
      if (!HasFiniteVoxelData(*channel))
      {
        throw std::invalid_argument("All fODF map channels must have finite voxel values.");
      }
    }
  }

  std::vector<float> PackPeak1Direction(const FodfVolumeChannels& channels)
  {
    const std::vector<float>& nufo = channels.NuFO.GetVoxels();
    const std::vector<float>& x = channels.Peak1X.GetVoxels();
    const std::vector<float>& y = channels.Peak1Y.GetVoxels();
    const std::vector<float>& z = channels.Peak1Z.GetVoxels();

    std::vector<float> packed(nufo.size() * 3U, 0.0f);
    for (size_t i = 0; i < nufo.size(); ++i)
    {
      if (nufo[i] <= 0.0f)
      {
        continue;
      }
      packed[i * 3U + 0U] = x[i];
      packed[i * 3U + 1U] = y[i];
      packed[i * 3U + 2U] = z[i];
    }
    return packed;
  }

  std::vector<float> PackNormalizedScalars(const FodfVolumeChannels& channels)
  {
    const std::vector<float>& afd = channels.AFD.GetVoxels();
    const std::vector<float>& amplitude1 = channels.Peak1Amplitude.GetVoxels();
    const std::vector<float>& amplitude2 = channels.Peak2Amplitude.GetVoxels();
    const std::vector<float>& amplitude3 = channels.Peak3Amplitude.GetVoxels();

    const float afdScale = PositivePercentile(afd, kNormalizationPercentile);
    const float amplitudeScale = PositivePercentile(amplitude1, kNormalizationPercentile);

    std::vector<float> packed(afd.size() * 4U, 0.0f);
    for (size_t i = 0; i < afd.size(); ++i)
    {
      packed[i * 4U + 0U] = NormalizeToUnit(afd[i], afdScale);
      packed[i * 4U + 1U] = NormalizeToUnit(amplitude1[i], amplitudeScale);
      packed[i * 4U + 2U] = NormalizeToUnit(amplitude2[i], amplitudeScale);
      packed[i * 4U + 3U] = NormalizeToUnit(amplitude3[i], amplitudeScale);
    }
    return packed;
  }

  std::vector<float> PackNormalizedPeakCount(const FodfVolumeChannels& channels)
  {
    const std::vector<float>& nufo = channels.NuFO.GetVoxels();
    std::vector<float> packed(nufo.size(), 0.0f);
    std::transform(nufo.begin(), nufo.end(), packed.begin(),
                   [](float count) { return NormalizeToUnit(count, kMaxPeakCount); });
    return packed;
  }
}

/**
 * @brief Validate the raw fODF maps and pack them into the three CPU texture payloads of the
 *        FodfVolume shader contract, normalizing for display. Pure CPU, so it runs on the
 *        preprocessing worker.
 *
 * @param channels raw maps from FodfPeakExtractionStage (or a loaded dataset)
 * @param cancellation checked before validation and before each packed texture
 */
FodfVolumeTextureData PackFodfVolumeTextures(const FodfVolumeChannels& channels,
                                             const CancellationToken& cancellation)
{
  cancellation.ThrowIfCancellationRequested();
  ValidateChannels(channels);

  FodfVolumeTextureData packed;
  packed.dimensions = channels.AFD.GetDimensions();
  packed.spacing = channels.AFD.GetSpacing();

  // Texture 0 (RGB): Peak 1 direction
  cancellation.ThrowIfCancellationRequested();
  packed.textures[0] = MakeTextureData(packed.dimensions, TextureFormat::RGB32F, PackPeak1Direction(channels));

  // Texture 1 (RGBA): (AFD_n, Peak1Amp_n, Peak2Amp_n, Peak3Amp_n)
  cancellation.ThrowIfCancellationRequested();
  packed.textures[1] = MakeTextureData(packed.dimensions, TextureFormat::RGBA32F, PackNormalizedScalars(channels));

  // Texture 2 (R): NuFO / 3
  cancellation.ThrowIfCancellationRequested();
  packed.textures[2] = MakeTextureData(packed.dimensions, TextureFormat::R32F, PackNormalizedPeakCount(channels));

  return packed;
}

/**
 * @brief Construct a new FodfVolume from already packed textures and already built render modes.
 *        Makes no GL calls: the textures upload lazily at their first bind on the GL thread.
 *
 * @param id
 * @param textures packed by PackFodfVolumeTextures
 * @param renderModes built by CreateRenderModes; must not be empty
 */
FodfVolume::FodfVolume(const std::string& id,
                       FodfVolumeTextureData textures,
                       std::vector<RenderMode> renderModes)
    : Volume(id, textures.dimensions, textures.spacing, renderModes.empty() ? nullptr : renderModes.front().shader)
{
  if (renderModes.empty())
  {
    EngineLog::Error("FodfVolume could not initialize any valid render mode shader.", kLogSource);
    throw std::runtime_error("FodfVolume could not initialize any valid render mode shader.");
  }

  for (TextureData3D& textureData : textures.textures)
  {
    textureSet.AddTexture(std::make_shared<Texture3D>(std::move(textureData), true));
  }

  this->renderModes = std::move(renderModes);

  EngineLog::Message(
    "FodfVolume ready: " + std::to_string(this->renderModes.size()) + "/2 render modes available",
    kLogSource);

  SetActiveRenderMode(0);
}

void FodfVolume::Apply(IUniformSink& shader) const
{
  Volume::Apply(shader);
  shader.SetInt("shader.selectedChannel", selectedChannel);
  ApplySlicePlaneUniforms(shader, slicePlane.get());
}

void FodfVolume::SetSlicePlane(std::shared_ptr<const SlicePlane> newPlane)
{
  slicePlane = std::move(newPlane);
}

/**
 * @brief Build the available render modes for an fODF volume. GL thread only (compiles shaders).
 *        Modes whose shader fails to build are skipped with a warning.
 */
std::vector<FodfVolume::RenderMode> FodfVolume::CreateRenderModes()
{
  std::vector<RenderMode> renderModes;

  std::shared_ptr<Shader> channelSliceShader = std::make_shared<Shader>(
      "fodf_channel_slice_shader",
      "shaders/volume/volume_vertex.glsl",
      "shaders/fodf_fragment_shaders/volume_fodf_channel_slice_fragment.glsl");
  if (channelSliceShader && channelSliceShader->IsValid())
  {
    renderModes.push_back(RenderMode{kChannelSliceModeLabel, channelSliceShader});
  }
  else
  {
    EngineLog::Warning(std::string("Render mode '") + kChannelSliceModeLabel + "' unavailable: shader failed to build",
                       kLogSource);
  }

  std::shared_ptr<Shader> peakRgbSliceShader = std::make_shared<Shader>(
      "fodf_peak_rgb_slice_shader",
      "shaders/volume/volume_vertex.glsl",
      "shaders/fodf_fragment_shaders/volume_fodf_peak_rgb_slice_fragment.glsl");
  if (peakRgbSliceShader && peakRgbSliceShader->IsValid())
  {
    (*peakRgbSliceShader)["shader.density"] = 1.0f;
    renderModes.push_back(RenderMode{kPeakRgbSliceModeLabel, peakRgbSliceShader});
  }
  else
  {
    EngineLog::Warning(std::string("Render mode '") + kPeakRgbSliceModeLabel + "' unavailable: shader failed to build",
                       kLogSource);
  }

  return renderModes;
}

bool FodfVolume::SetActiveRenderMode(int modeIndex)
{
  if (modeIndex < 0 || modeIndex >= static_cast<int>(renderModes.size()))
  {
    EngineLog::Warning("SetActiveRenderMode: index out of range", kLogSource);
    return false;
  }

  const RenderMode& mode = renderModes[static_cast<size_t>(modeIndex)];
  if (!mode.shader || !mode.shader->IsValid())
  {
    EngineLog::Warning("SetActiveRenderMode: render mode '" + mode.label + "' has no built shader", kLogSource);
    return false;
  }

  selectedRenderMode = modeIndex;
  shader = mode.shader; // Draw() uses the base shader.
  return true;
}

bool FodfVolume::SetSelectedRenderModeIndex(int modeIndex)
{
  return SetActiveRenderMode(modeIndex);
}

// By label rather than index: a mode whose shader failed to build is missing from renderModes.
bool FodfVolume::IsChannelSliceModeActive() const
{
  return selectedRenderMode >= 0 && selectedRenderMode < static_cast<int>(renderModes.size()) &&
         renderModes[static_cast<size_t>(selectedRenderMode)].label == kChannelSliceModeLabel;
}

std::vector<InspectFieldPtr> FodfVolume::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields = Volume::GetInspectFields();

  std::vector<std::string> renderModeOptions;
  for (const RenderMode& mode : renderModes)
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

  if (IsChannelSliceModeActive())
  {
    const auto currentChannel = [this]() -> std::string
    {
      return selectedChannel >= 0 && selectedChannel < static_cast<int>(kChannelOptions.size())
        ? kChannelOptions[static_cast<size_t>(selectedChannel)]
        : std::string();
    };
    auto channelField = MakeInspectField(
        "selectedChannel", "Selected Channel", "Visualization", InspectFieldType::Dropdown,
        currentChannel(),
        [currentChannel]() -> InspectValue { return currentChannel(); },
        [this](const InspectValue& value)
        {
          const auto* newValue = std::get_if<std::string>(&value);
          if (!newValue) return;
          const auto match = std::find(kChannelOptions.begin(), kChannelOptions.end(), *newValue);
          if (match != kChannelOptions.end())
          {
            selectedChannel = static_cast<int>(std::distance(kChannelOptions.begin(), match));
          }
        });
    channelField->enumOptions = kChannelOptions;
    fields.push_back(channelField);
  }

  return fields;
}
