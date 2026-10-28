#pragma once

#include "Volume/Volume.h"
#include "render/VolumeTextureSet.h"

class FloatVolume final : public Volume
{
public:
  explicit FloatVolume(const std::string& id,
                       const VolumeData& volumeData,
                       std::shared_ptr<Shader> shader);

};