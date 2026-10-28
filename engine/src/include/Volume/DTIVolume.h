#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Volume/Volume.h"
#include "Render/VolumeTextureSet.h"
#include "Render/Texture/TextureData3D.h"
#include "foundation/threading/Cancellation.h"

struct DTIVolumeChannels
{
  // The 6 unique channels of a DTI volume. The shader will reconstruct the full tensor from these.
  VolumeData Dxx; //                    
  VolumeData Dyy; //       [ Dxx, Dxy, Dxz ]  
  VolumeData Dzz; //   D = [ Dxy, Dyy, Dyz ]
  VolumeData Dxy; //       [ Dxz, Dyz, Dzz ] 
  VolumeData Dxz; //          
  VolumeData Dyz; //   

  // Principal eigenvector components synthesized from the tensor.
  VolumeData EVx;
  VolumeData EVy;
  VolumeData EVz;

  VolumeData L1;
  VolumeData L2;
  VolumeData L3;

  // Derived scalar maps synthesized from the tensor channels.
  VolumeData FA;
  VolumeData MD;
  VolumeData AD;
  VolumeData RD;
};

// CPU-packed DTIVolume textures, ready for the lazy Texture3D ctor. Graphics-API free.
struct DTIVolumeTextureData
{
  glm::ivec3 dimensions{0};
  glm::vec3 spacing{1.0f};
  std::array<TextureData3D, 5> textures; // index i == volumeTextures[i] == texture unit 8 + i
};

// Pure CPU, safe on any thread (e.g. the preprocessing worker). Validates that all channels share
// dimensions/spacing and hold finite voxels (std::invalid_argument otherwise), then packs them per
// the shader contract: [0] (Dxx,Dyy,Dzz), [1] (Dxy,Dxz,Dyz), [2] (EVx,EVy,EVz), [3] (L1,L2,L3) as
// RGB32F and [4] (FA,MD,AD,RD) as RGBA32F. Throws OperationCancelled between textures on request.
DTIVolumeTextureData PackDTIVolumeTextures(const DTIVolumeChannels& channels,
                                           const CancellationToken& cancellation = {});

class DTIVolume final : public Volume
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
  DTIVolume(const std::string& id,
            DTIVolumeTextureData textures,
            std::vector<RenderMode> renderModes);

  void Apply(IUniformSink &shader) const override;
  
  int GetSelectedRenderModeIndex() const { return selectedRenderMode; }
  int GetSelectedChannelIndex() const { return selectedChannel; }
  bool SetSelectedRenderModeIndex(int modeIndex);
  void SetSelectedChannelIndex(int channelIndex) { selectedChannel = channelIndex; }

  std::vector<InspectFieldPtr> GetInspectFields() override;
  
private:
  bool SetActiveRenderMode(int modeIndex);

  const RenderMode *GetActiveRenderMode() const;

  int selectedChannel = 0;
  int selectedRenderMode = 0;
  std::vector<RenderMode> renderModes;

  enum SelectedChannelIndex
  {
    Dxx = 0,
    Dyy = 1,
    Dzz = 2,
    Dxy = 3,
    Dxz = 4,
    Dyz = 5,
    EVx = 6,
    EVy = 7,
    EVz = 8,
    FA = 9,
    MD = 10,
    AD = 11,
    RD = 12,
    L1 = 13,
    L2 = 14,
    L3 = 15
  };
};