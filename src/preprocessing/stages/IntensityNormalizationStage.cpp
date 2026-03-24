#include "Preprocessing/stages/IntensityNormalizationStage.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Preprocessing/MriPreprocessingStages.h"

const char* IntensityNormalizationStage::Name() const
{
  return "Intensity normalization";
}

void IntensityNormalizationStage::Execute(MriPreprocessingContext& context) const
{
  const VolumeMetadata& metadata = context.loadedVolume.GetMetadata();
  context.normalizedVolume = VolumeData<float>(
    metadata.dimensions.x,
    metadata.dimensions.y,
    metadata.dimensions.z,
    metadata.spacing);

  const std::vector<float>& source = context.loadedVolume.GetVoxels();
  std::vector<float>& target = context.normalizedVolume.GetVoxels();

  if (source.empty())
  {
    throw std::runtime_error("Loaded MRI volume has no voxels.");
  }

  const auto [minIt, maxIt] = std::minmax_element(source.begin(), source.end());
  const float minV = *minIt;
  const float maxV = *maxIt;
  const float range = maxV - minV;

  if (!std::isfinite(minV) || !std::isfinite(maxV))
  {
    throw std::runtime_error("Loaded MRI volume contains non-finite voxel values.");
  }

  if (range <= std::numeric_limits<float>::epsilon())
  {
    std::fill(target.begin(), target.end(), 0.0f);
    context.report.warnings.push_back("Input volume has near-constant intensity; normalized output was flattened.");
    return;
  }

  for (size_t i = 0; i < source.size(); ++i)
  {
    target[i] = std::clamp((source[i] - minV) / range, 0.0f, 1.0f);
  }
}

std::unique_ptr<IMriPreprocessingStage> CreateIntensityNormalizationStage()
{
  return std::make_unique<IntensityNormalizationStage>();
}
