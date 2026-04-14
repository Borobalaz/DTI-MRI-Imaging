#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Volume.h"
#include "VolumeTextureSet.h"

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

  // Binary skull extraction mask (1 = keep voxel, 0 = reject voxel).
  VolumeData Mask;
};

class DTIVolume final : public Volume
{
public:
  explicit DTIVolume(DTIVolumeChannels channels,
                     std::shared_ptr<Shader> shader);

  void Apply(Shader &shader) const override;
  void Draw(const UniformProvider &frameUniforms) const override;
  void CollectInspectableFields(std::vector<UiField> &out, const std::string &groupPrefix) override;
  
  void GetMajorEigenVectorAt(glm::ivec3 voxelCoord, glm::vec3 &outVector) const;

  // Register this volume's shaders with a scene for hot reload tracking
  void RegisterShadersWithScene(class Scene* scene);
  
private:
  struct RenderMode
  {
    std::string label;
    std::shared_ptr<Shader> shader;
  };

  void InitializeRenderModes();
  bool SetActiveRenderMode(int modeIndex);

  const RenderMode *GetActiveRenderMode() const;

  DTIVolumeChannels channels;
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