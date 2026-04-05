#pragma once

#include <array>
#include <optional>
#include <string>

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
  
private:
  DTIVolumeChannels channels;
};