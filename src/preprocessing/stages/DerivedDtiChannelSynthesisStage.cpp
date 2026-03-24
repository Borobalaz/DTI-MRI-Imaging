#include "Preprocessing/stages/DerivedDtiChannelSynthesisStage.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "Preprocessing/MriPreprocessingStages.h"

namespace
{
  inline size_t FlatIndex(int x, int y, int z, int width, int height)
  {
    return static_cast<size_t>(z) * static_cast<size_t>(height) * static_cast<size_t>(width) +
      static_cast<size_t>(y) * static_cast<size_t>(width) +
      static_cast<size_t>(x);
  }

}

const char* DerivedDtiChannelSynthesisStage::Name() const
{
  return "Derived DTI channel synthesis";
}

void DerivedDtiChannelSynthesisStage::Execute(MriPreprocessingContext& context) const
{
  const VolumeMetadata& metadata = context.normalizedVolume.GetMetadata();
  const int width = metadata.dimensions.x;
  const int height = metadata.dimensions.y;
  const int depth = metadata.dimensions.z;

  auto sample = [&](int x, int y, int z) -> float
  {
    const int clampedX = std::clamp(x, 0, width - 1);
    const int clampedY = std::clamp(y, 0, height - 1);
    const int clampedZ = std::clamp(z, 0, depth - 1);
    return context.normalizedVolume.GetVoxels()[FlatIndex(clampedX, clampedY, clampedZ, width, height)];
  };

  VolumeData<float> fa(width, height, depth, metadata.spacing);
  VolumeData<float> md(width, height, depth, metadata.spacing);
  VolumeData<float> ad(width, height, depth, metadata.spacing);
  VolumeData<float> rd(width, height, depth, metadata.spacing);

  std::vector<float>& faVoxels = fa.GetVoxels();
  std::vector<float>& mdVoxels = md.GetVoxels();
  std::vector<float>& adVoxels = ad.GetVoxels();
  std::vector<float>& rdVoxels = rd.GetVoxels();

  float maxGradientMagnitude = std::numeric_limits<float>::epsilon();
  float maxAd = std::numeric_limits<float>::epsilon();
  float maxRd = std::numeric_limits<float>::epsilon();

  for (int z = 0; z < depth; ++z)
  {
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        const size_t index = FlatIndex(x, y, z, width, height);

        const float center = sample(x, y, z);
        const float gx = 0.5f * (sample(x + 1, y, z) - sample(x - 1, y, z));
        const float gy = 0.5f * (sample(x, y + 1, z) - sample(x, y - 1, z));
        const float gz = 0.5f * (sample(x, y, z + 1) - sample(x, y, z - 1));

        const float gradMag = std::sqrt(gx * gx + gy * gy + gz * gz);
        const float adProxy = std::fabs(gx);
        const float rdProxy = std::sqrt(gy * gy + gz * gz);

        faVoxels[index] = gradMag;
        mdVoxels[index] = center;
        adVoxels[index] = adProxy;
        rdVoxels[index] = rdProxy;

        maxGradientMagnitude = std::max(maxGradientMagnitude, gradMag);
        maxAd = std::max(maxAd, adProxy);
        maxRd = std::max(maxRd, rdProxy);
      }
    }
  }

  const float faNorm = std::max(maxGradientMagnitude, std::numeric_limits<float>::epsilon());
  const float adNorm = std::max(maxAd, std::numeric_limits<float>::epsilon());
  const float rdNorm = std::max(maxRd, std::numeric_limits<float>::epsilon());

  for (size_t i = 0; i < faVoxels.size(); ++i)
  {
    faVoxels[i] = std::clamp(faVoxels[i] / faNorm, 0.0f, 1.0f);
    adVoxels[i] = std::clamp(adVoxels[i] / adNorm, 0.0f, 1.0f);
    rdVoxels[i] = std::clamp(rdVoxels[i] / rdNorm, 0.0f, 1.0f);
  }

  context.outputChannels.fa = std::move(fa);

  if (context.request.generateAllChannels)
  {
    context.outputChannels.md = std::move(md);
    context.outputChannels.ad = std::move(ad);
    context.outputChannels.rd = std::move(rd);
  }

  context.report.warnings.push_back(
    "DTI channels were synthesized from structural/functional MRI intensity gradients as preprocessing proxies, not from diffusion tensors.");
}

std::unique_ptr<IMriPreprocessingStage> CreateDerivedDtiChannelSynthesisStage()
{
  return std::make_unique<DerivedDtiChannelSynthesisStage>();
}
