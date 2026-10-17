#include "Preprocessing/stages/DWINormalizationStage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
  void NormalizeMinMaxChannel(std::vector<float> &voxels)
  {
    float minValue = std::numeric_limits<float>::infinity();
    float maxValue = -std::numeric_limits<float>::infinity();

    for (float value : voxels)
    {
      if (!std::isfinite(value))
      {
        continue;
      }

      minValue = std::min(minValue, value);
      maxValue = std::max(maxValue, value);
    }

    if (!std::isfinite(minValue) || !std::isfinite(maxValue) || !(maxValue > minValue))
    {
      for (float &value : voxels)
      {
        if (std::isfinite(value))
        {
          value = 0.0f;
        }
      }

      return;
    }

    const float range = maxValue - minValue;

    for (float &value : voxels)
    {
      if (std::isfinite(value))
      {
        value = std::clamp((value - minValue) / range, 0.0f, 1.0f);
      }
    }
  }
}

/**
 * @brief Get the name of the preprocessing stage.
 *
 * @return std::string
 */
const char *DWINormalizationStage::Name() const
{
  return "DWI visualization normalization";
}

/**
 * @brief Normalize DTI channels to [0, 1] range.
 *
 * @param context
 */
void DWINormalizationStage::Execute(MriPreprocessingContext &context) const
{
  auto &channels = context.outputDtiChannels;
  const std::array<VolumeData *, 16> allChannels = {
      &channels.Dxx, &channels.Dyy, &channels.Dzz,
      &channels.Dxy, &channels.Dxz, &channels.Dyz,
      &channels.EVx, &channels.EVy, &channels.EVz,
      &channels.FA, &channels.MD, &channels.AD, &channels.RD,
      &channels.L1, &channels.L2, &channels.L3};

  for (VolumeData *channel : allChannels)
  {
    NormalizeMinMaxChannel(channel->GetVoxels());
  }
}

/**
 * @brief Create a Dwi Normalization Stage object
 *
 * @return std::unique_ptr<IMriPreprocessingStage>
 */
std::unique_ptr<IMriPreprocessingStage> CreateDwiNormalizationStage()
{
  return std::make_unique<DWINormalizationStage>();
}