#include "Preprocessing/stages/DWINormalizationStage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
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

  void LogResults(const std::vector<float> &voxels)
  {
    float minValue = std::numeric_limits<float>::infinity();
    float maxValue = -std::numeric_limits<float>::infinity();
    std::vector<float> finiteValues;
    finiteValues.reserve(voxels.size());

    for (float value : voxels)
    {
      if (!std::isfinite(value))
      {
        continue;
      }

      finiteValues.push_back(value);
      minValue = std::min(minValue, value);
      maxValue = std::max(maxValue, value);
    }

    if (finiteValues.empty())
    {
      std::cout << "Channel stats: min=nan, max=nan, median=nan" << std::endl;
      return;
    }

    std::sort(finiteValues.begin(), finiteValues.end());

    const size_t count = finiteValues.size();
    const float median = (count % 2 == 0)
                             ? 0.5f * (finiteValues[(count / 2) - 1] + finiteValues[count / 2])
                             : finiteValues[count / 2];

    std::cout << "Channel stats: min=" << minValue << ", max=" << maxValue << ", median=" << median << std::endl;
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
  auto &channels = context.outputChannels;
  const std::array<std::pair<const char *, VolumeData *>, 16> allChannels = {
      std::make_pair("Dxx", &channels.Dxx),
      std::make_pair("Dyy", &channels.Dyy),
      std::make_pair("Dzz", &channels.Dzz),
      std::make_pair("Dxy", &channels.Dxy),
      std::make_pair("Dxz", &channels.Dxz),
      std::make_pair("Dyz", &channels.Dyz),
      std::make_pair("EVx", &channels.EVx),
      std::make_pair("EVy", &channels.EVy),
      std::make_pair("EVz", &channels.EVz),
      std::make_pair("FA", &channels.FA),
      std::make_pair("MD", &channels.MD),
      std::make_pair("AD", &channels.AD),
      std::make_pair("RD", &channels.RD),
      std::make_pair("L1", &channels.L1),
      std::make_pair("L2", &channels.L2),
      std::make_pair("L3", &channels.L3)};

  for (const auto &[channelName, channel] : allChannels)
  {
    //std::cout << "Normalizing " << channelName << "..." << std::endl;
    NormalizeMinMaxChannel(channel->GetVoxels());
  }

  //LogResults(channels.Dxx.GetVoxels());
  //LogResults(channels.Dyy.GetVoxels());
  //LogResults(channels.Dzz.GetVoxels());
  //LogResults(channels.Dxy.GetVoxels());
  //LogResults(channels.Dxz.GetVoxels());
  //LogResults(channels.Dyz.GetVoxels());
  //LogResults(channels.FA.GetVoxels());
  //LogResults(channels.MD.GetVoxels());
  //LogResults(channels.AD.GetVoxels());
  //LogResults(channels.RD.GetVoxels());
  //LogResults(channels.L1.GetVoxels());
  //LogResults(channels.L2.GetVoxels());
  //LogResults(channels.L3.GetVoxels());
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