#include "preprocessing/stages/DWINormalizationStage.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

/**
 * @brief Get the name of the preprocessing stage.
 * 
 * @return std::string 
 */
const char* DWINormalizationStage::Name() const
{
  return "DWI signal normalization";
}

/**
 * @brief Normalize each output channel independently to [0, 1] range
 * 
 * @param context 
 */
void DWINormalizationStage::Execute(MriPreprocessingContext& context) const
{
  // Array of pointers to each channel for iteration
  std::vector<VolumeData*> channels = {
    &context.outputChannels.Dxx,
    &context.outputChannels.Dyy,
    &context.outputChannels.Dzz,
    &context.outputChannels.Dxy,
    &context.outputChannels.Dxz,
    &context.outputChannels.Dyz
  };

  for (auto* channel : channels)
  {
    auto& voxels = channel->GetVoxels();
    
    // Find min and max values in this channel
    float minVal = std::numeric_limits<float>::max();
    float maxVal = std::numeric_limits<float>::lowest();
    
    for (float v : voxels)
    {
      if (std::isfinite(v))
      {
        minVal = std::min(minVal, v);
        maxVal = std::max(maxVal, v);
      }
    }
    
    // Normalize to [0, 1]
    float range = maxVal - minVal;
    if (range > 0.0f)
    {
      for (float& v : voxels)
      {
        if (std::isfinite(v))
        {
          v = (v - minVal) / range;
        }
      }
    }
    else
    {
      // If all values are the same, set to 0
      for (float& v : voxels)
      {
        if (std::isfinite(v))
        {
          v = 0.0f;
        }
      }
    }
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