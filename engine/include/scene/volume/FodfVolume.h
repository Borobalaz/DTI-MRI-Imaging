#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "scene/volume/Volume.h"
#include "scene/volume/VolumeData.h"
#include "render/texture/TextureData3D.h"
#include "foundation/threading/Cancellation.h"

class SlicePlane;

// Per-voxel fODF peak maps produced by FodfPeakExtractionStage. All channels are raw (not
// normalized): AFD = c00 * 2 * sqrt(pi), NuFO = number of peaks at or above the tracking cutoff,
// PeakK* = unit direction and SH amplitude of the k-th strongest kept peak (zero when absent).
struct FodfVolumeChannels
{
  VolumeData AFD;
  VolumeData NuFO;

  VolumeData Peak1X;
  VolumeData Peak1Y;
  VolumeData Peak1Z;

  VolumeData Peak2X;
  VolumeData Peak2Y;
  VolumeData Peak2Z;

  VolumeData Peak3X;
  VolumeData Peak3Y;
  VolumeData Peak3Z;

  VolumeData Peak1Amplitude;
  VolumeData Peak2Amplitude;
  VolumeData Peak3Amplitude;
};

// CPU-packed FodfVolume textures, ready for the lazy Texture3D ctor. Graphics-API free.
struct FodfVolumeTextureData
{
  glm::ivec3 dimensions{0};
  glm::vec3 spacing{1.0f};
  std::array<TextureData3D, 3> textures; // index i == volumeTextures[i] == texture unit 8 + i
};

// Pure CPU, safe on any thread (e.g. the preprocessing worker). Validates that all channels share
// dimensions/spacing and hold finite voxels (std::invalid_argument otherwise), then packs them per
// the shader contract:
//   [0] RGB32F  (Peak1X, Peak1Y, Peak1Z), raw signed unit direction, zero where NuFO == 0
//   [1] RGBA32F (AFD_n, Peak1Amp_n, Peak2Amp_n, Peak3Amp_n)
//   [2] R32F    NuFO / 3
// AFD_n = AFD / p99(AFD > 0); the three amplitudes share one scale, p99 of the positive Peak1
// amplitudes, so the peaks stay comparable. Both are clamped to [0, 1]. Throws OperationCancelled
// between textures on request.
FodfVolumeTextureData PackFodfVolumeTextures(const FodfVolumeChannels& channels,
                                             const CancellationToken& cancellation = {});

class FodfVolume final : public Volume
{
public:
  struct RenderMode
  {
    std::string label;
    std::shared_ptr<Shader> shader;
  };

  // GL thread only: compiles one shader per render mode and returns the modes whose shader built.
  static std::vector<RenderMode> CreateRenderModes();

  // No GL calls and no heavy CPU work: the textures upload lazily at first bind. Throws
  // std::runtime_error when renderModes is empty.
  FodfVolume(const std::string& id,
             FodfVolumeTextureData textures,
             std::vector<RenderMode> renderModes);

  // Channel Slice channel shown by default: AFD (index into the "Selected Channel" dropdown order).
  static constexpr int kDefaultChannelIndex = 0;

  void Apply(IUniformSink &shader) const override;

  int GetSelectedRenderModeIndex() const { return selectedRenderMode; }
  int GetSelectedChannelIndex() const { return selectedChannel; }
  bool SetSelectedRenderModeIndex(int modeIndex);
  void SetSelectedChannelIndex(int channelIndex) { selectedChannel = channelIndex; }

  // Shared scene plane used by both slice render modes. Read in Apply on the engine thread under
  // the scene lock. Null means the default axial plane through the centre.
  void SetSlicePlane(std::shared_ptr<const SlicePlane> newPlane);

  std::vector<InspectFieldPtr> GetInspectFields() override;

private:
  bool SetActiveRenderMode(int modeIndex);
  bool IsChannelSliceModeActive() const;

  int selectedChannel = kDefaultChannelIndex;
  int selectedRenderMode = 0;
  std::vector<RenderMode> renderModes;
  std::shared_ptr<const SlicePlane> slicePlane;
};
