#include "Preprocessing/stages/ScalarVolumeLoadStage.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Preprocessing/MriPreprocessingStages.h"
#include "VolumeFileLoader.h"

namespace
{
  VolumeData<float> ConvertToFloatVolume(const VolumeData<uint8_t>& source)
  {
    const VolumeMetadata& metadata = source.GetMetadata();
    VolumeData<float> converted(metadata.dimensions.x, metadata.dimensions.y, metadata.dimensions.z, metadata.spacing);
    std::vector<float>& voxels = converted.GetVoxels();
    const std::vector<uint8_t>& sourceVoxels = source.GetVoxels();

    for (size_t i = 0; i < voxels.size(); ++i)
    {
      voxels[i] = static_cast<float>(sourceVoxels[i]);
    }

    return converted;
  }

  VolumeData<float> ConvertToFloatVolume(const VolumeData<uint16_t>& source)
  {
    const VolumeMetadata& metadata = source.GetMetadata();
    VolumeData<float> converted(metadata.dimensions.x, metadata.dimensions.y, metadata.dimensions.z, metadata.spacing);
    std::vector<float>& voxels = converted.GetVoxels();
    const std::vector<uint16_t>& sourceVoxels = source.GetVoxels();

    for (size_t i = 0; i < voxels.size(); ++i)
    {
      voxels[i] = static_cast<float>(sourceVoxels[i]);
    }

    return converted;
  }

}

const char* ScalarVolumeLoadStage::Name() const
{
  return "Scalar volume load";
}

void ScalarVolumeLoadStage::Execute(MriPreprocessingContext& context) const
{
  if (context.selectedSourceVolumePath.empty())
  {
    throw std::runtime_error("No source volume selected before loading.");
  }

  if (const std::optional<VolumeData<float>> loadedF32 =
    VolumeFileLoader::LoadTyped<float>(context.selectedSourceVolumePath))
  {
    context.loadedVolume = *loadedF32;
    return;
  }

  if (const std::optional<VolumeData<uint16_t>> loadedU16 =
    VolumeFileLoader::LoadTyped<uint16_t>(context.selectedSourceVolumePath))
  {
    context.loadedVolume = ConvertToFloatVolume(*loadedU16);
    return;
  }

  if (const std::optional<VolumeData<uint8_t>> loadedU8 =
    VolumeFileLoader::LoadTyped<uint8_t>(context.selectedSourceVolumePath))
  {
    context.loadedVolume = ConvertToFloatVolume(*loadedU8);
    return;
  }

  throw std::runtime_error(
    "Failed to load selected source volume as scalar data: " + context.selectedSourceVolumePath +
    " | VolumeLoader error: " + VolumeFileLoader::GetLastError());
}

std::unique_ptr<IMriPreprocessingStage> CreateScalarVolumeLoadStage()
{
  return std::make_unique<ScalarVolumeLoadStage>();
}
