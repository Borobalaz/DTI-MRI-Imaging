#include "Preprocessing/stages/fodf/FodfPeakExtractionStage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "FodfCsdMath.h"
#include "Preprocessing/MriFodfTractographySettings.h"
#include "Preprocessing/PreprocessingParallel.h"
#include "Preprocessing/stages/fodf/FodfPreprocessingStages.h"
#include "foundation/log/EngineLog.h"
#include "scene/glyphs/SHVolume.h"
#include "scene/spherical_harmonics/RealSH.h"
#include "scene/volume/FodfVolume.h"
#include "scene/volume/VolumeData.h"

namespace
{
  constexpr size_t kMaxPeaksPerVoxel = 3;
  constexpr double kCutoffReferencePercentile = 0.95;

  // Voxel buffers of the k-th peak's maps.
  struct PeakChannelVoxels
  {
    std::vector<float>* x;
    std::vector<float>* y;
    std::vector<float>* z;
    std::vector<float>* amplitude;
  };

  FodfVolumeChannels AllocateMaps(const glm::ivec3& dimensions, const glm::vec3& spacing)
  {
    auto make = [&]() { return VolumeData(dimensions.x, dimensions.y, dimensions.z, spacing); };
    FodfVolumeChannels maps;
    maps.AFD = make();
    maps.NuFO = make();
    maps.Peak1X = make();
    maps.Peak1Y = make();
    maps.Peak1Z = make();
    maps.Peak2X = make();
    maps.Peak2Y = make();
    maps.Peak2Z = make();
    maps.Peak3X = make();
    maps.Peak3Y = make();
    maps.Peak3Z = make();
    maps.Peak1Amplitude = make();
    maps.Peak2Amplitude = make();
    maps.Peak3Amplitude = make();
    return maps;
  }

  std::array<PeakChannelVoxels, kMaxPeaksPerVoxel> GetPeakChannelVoxels(FodfVolumeChannels& maps)
  {
    return {{
      {&maps.Peak1X.GetVoxels(), &maps.Peak1Y.GetVoxels(), &maps.Peak1Z.GetVoxels(), &maps.Peak1Amplitude.GetVoxels()},
      {&maps.Peak2X.GetVoxels(), &maps.Peak2Y.GetVoxels(), &maps.Peak2Z.GetVoxels(), &maps.Peak2Amplitude.GetVoxels()},
      {&maps.Peak3X.GetVoxels(), &maps.Peak3Y.GetVoxels(), &maps.Peak3Z.GetVoxels(), &maps.Peak3Amplitude.GetVoxels()},
    }};
  }

  // CSD-solved voxel: c00 > 0 and every coefficient finite.
  bool IsSupportVoxel(const float* coefficients, size_t coefficientCount)
  {
    if (!std::isfinite(coefficients[0]) || !(coefficients[0] > 0.0f))
    {
      return false;
    }
    return std::all_of(coefficients, coefficients + coefficientCount, [](float value) { return std::isfinite(value); });
  }

  // values is taken by value because nth_element reorders it; precondition: not empty.
  float Percentile(std::vector<float> values, double fraction)
  {
    const size_t rank = static_cast<size_t>(fraction * static_cast<double>(values.size() - 1));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(rank), values.end());
    return values[rank];
  }

  void ClearPeak(const PeakChannelVoxels& peak, size_t voxelIndex)
  {
    (*peak.x)[voxelIndex] = 0.0f;
    (*peak.y)[voxelIndex] = 0.0f;
    (*peak.z)[voxelIndex] = 0.0f;
    (*peak.amplitude)[voxelIndex] = 0.0f;
  }
}

/**
 * @brief Get the name of the preprocessing stage.
 *
 * @return const char*
 */
const char* FodfPeakExtractionStage::Name() const
{
  return "fODF peak extraction";
}

/**
 * @brief Find up to 3 peaks per CSD-solved voxel, derive the absolute amplitude cutoff from the
 *        95th percentile of peak-1 amplitude, drop peaks below it and store the raw fODF maps.
 *
 * @param context
 */
void FodfPeakExtractionStage::Execute(MriPreprocessingContext& context) const
{
  const std::shared_ptr<const SHVolume> sh = context.outputFodfShVolume;
  if (!sh || !sh->IsWellFormed())
  {
    throw std::runtime_error("fODF peak extraction: no valid fODF SH volume; fODF CSD must run first.");
  }

  const MriFodfTractographySettings settings =
    context.request.fodfTractographySettings ? *context.request.fodfTractographySettings : MriFodfTractographySettings{};

  const FodfCsdMath::PeakFinder finder(FodfCsdMath::BuildPeakSampleDirections(), sh->lmax,
                                       FodfCsdMath::kPeakNeighbourAngleDeg, FodfCsdMath::kPeakSeparationDeg);

  const int width = sh->dimensions.x;
  const int height = sh->dimensions.y;
  const int depth = sh->dimensions.z;
  const size_t coefficientCount = sh->CoefficientsPerVoxel();
  const double afdScale = 2.0 * std::sqrt(glm::pi<double>());

  FodfVolumeChannels maps = AllocateMaps(sh->dimensions, sh->spacing);
  std::vector<float>& afd = maps.AFD.GetVoxels();
  std::vector<float>& nufo = maps.NuFO.GetVoxels();
  const std::array<PeakChannelVoxels, kMaxPeaksPerVoxel> peakChannels = GetPeakChannelVoxels(maps);

  ParallelForSlices(context, depth, [&](int z)
  {
    FodfCsdMath::PeakFinder::Workspace workspace(finder);
    double coefficients[RealSH::kSHMaxCoefficients];
    FodfCsdMath::SHPeak found[kMaxPeaksPerVoxel];

    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        const float* voxelCoefficients = sh->VoxelCoefficients(x, y, z);
        if (!IsSupportVoxel(voxelCoefficients, coefficientCount))
        {
          continue;
        }
        std::copy(voxelCoefficients, voxelCoefficients + coefficientCount, coefficients);

        const size_t i = VolumeData::FlatIndex(x, y, z, width, height);
        afd[i] = static_cast<float>(coefficients[0] * afdScale);

        const size_t peakCount = finder.Find(coefficients, kMaxPeaksPerVoxel, workspace, found);
        for (size_t k = 0; k < peakCount; ++k)
        {
          (*peakChannels[k].x)[i] = static_cast<float>(found[k].direction.x);
          (*peakChannels[k].y)[i] = static_cast<float>(found[k].direction.y);
          (*peakChannels[k].z)[i] = static_cast<float>(found[k].direction.z);
          (*peakChannels[k].amplitude)[i] = static_cast<float>(found[k].amplitude);
        }
      }
    }
  });

  std::vector<float> peak1Amplitudes;
  for (const float amplitude : *peakChannels[0].amplitude)
  {
    if (amplitude > 0.0f)
    {
      peak1Amplitudes.push_back(amplitude);
    }
  }
  if (peak1Amplitudes.empty())
  {
    throw std::runtime_error("fODF peak extraction: no voxel has an fODF peak.");
  }

  const float referenceAmplitude = Percentile(std::move(peak1Amplitudes), kCutoffReferencePercentile);
  const float cutoff = settings.GetRelativeAmplitudeCutoff() * referenceAmplitude;

  // Peaks are strongest first, so the kept ones form a prefix; NuFO counts it.
  std::array<size_t, kMaxPeaksPerVoxel + 1> nufoHistogram{};
  for (size_t i = 0; i < afd.size(); ++i)
  {
    if (!(afd[i] > 0.0f))
    {
      continue;
    }

    size_t keptCount = 0;
    while (keptCount < kMaxPeaksPerVoxel)
    {
      const float amplitude = (*peakChannels[keptCount].amplitude)[i];
      if (!(amplitude > 0.0f) || amplitude < cutoff)
      {
        break;
      }
      ++keptCount;
    }
    for (size_t k = keptCount; k < kMaxPeaksPerVoxel; ++k)
    {
      ClearPeak(peakChannels[k], i);
    }

    nufo[i] = static_cast<float>(keptCount);
    ++nufoHistogram[keptCount];
  }

  EngineLog::Message(
    "fODF peaks: p95 peak-1 amplitude " + FodfCsdMath::FormatScientific(referenceAmplitude) + ", cutoff " +
    FodfCsdMath::FormatScientific(cutoff) + " (relative " +
    FodfCsdMath::FormatFixed(settings.GetRelativeAmplitudeCutoff(), 2) + "); NuFO histogram 0: " +
    std::to_string(nufoHistogram[0]) + ", 1: " + std::to_string(nufoHistogram[1]) + ", 2: " +
    std::to_string(nufoHistogram[2]) + ", 3: " + std::to_string(nufoHistogram[3]),
    "FodfPeaks");

  context.fodfAbsoluteAmplitudeCutoff = cutoff;
  context.outputFodfMaps = std::make_shared<const FodfVolumeChannels>(std::move(maps));
}

/**
 * @brief Create a Fodf Peak Extraction Stage object
 *
 * @return std::unique_ptr<IMriPreprocessingStage>
 */
std::unique_ptr<IMriPreprocessingStage> CreateFodfPeakExtractionStage()
{
  return std::make_unique<FodfPeakExtractionStage>();
}
