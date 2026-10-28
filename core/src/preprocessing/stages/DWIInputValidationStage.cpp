#include "Preprocessing/stages/DWIInputValidationStage.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Preprocessing/stages/CommonPreprocessingStages.h"
#include "scene/volume/VolumeFileLoader.h"
#include "scene/volume/VolumeSeriesData.h"

/**
 * @brief Get the name of the preprocessing stage.
 * @return The name of the stage.
 */
const char* DWIInputValidationStage::Name() const
{
  return "DWI input validation";
}

/**
 * @brief Validate the existence and basic integrity of the DWI volume and its associated bval/bvec files. 
 *        If validation passes, populate the context with the resolved file paths and source volume metadata,
 *        and caches the loaded DWI series in context.dwiSeries for later stages.
 * 
 * @param context 
 */
void DWIInputValidationStage::Execute(MriPreprocessingContext& context) const
{
  if (context.request.dwiVolumePath.empty())
  {
    throw std::runtime_error("DWI volume path is empty.");
  }

  if (context.request.bvalPath.empty())
  {
    throw std::runtime_error("bval sidecar path is empty.");
  }

  if (context.request.bvecPath.empty())
  {
    throw std::runtime_error("bvec sidecar path is empty.");
  }

  const std::filesystem::path dwiPath(context.request.dwiVolumePath);
  const std::filesystem::path bvalPath(context.request.bvalPath);
  const std::filesystem::path bvecPath(context.request.bvecPath);

  if (!std::filesystem::exists(dwiPath))
  {
    throw std::runtime_error("DWI volume file does not exist: " + dwiPath.string());
  }

  if (!std::filesystem::exists(bvalPath))
  {
    throw std::runtime_error("bval sidecar file does not exist: " + bvalPath.string());
  }

  if (!std::filesystem::exists(bvecPath))
  {
    throw std::runtime_error("bvec sidecar file does not exist: " + bvecPath.string());
  }

  context.selectedDwiVolumePath = dwiPath.string();
  context.selectedBvalPath = bvalPath.string();
  context.selectedBvecPath = bvecPath.string();
  context.report.sourceVolumePath = context.selectedDwiVolumePath;

  std::optional<VolumeSeriesData> dwiSeries = VolumeFileLoader::LoadSeries(context.selectedDwiVolumePath);
  if (!dwiSeries.has_value())
  {
    throw std::runtime_error(
      "Failed to load DWI series from '" + context.selectedDwiVolumePath +
      "'. loaderError='" + VolumeFileLoader::GetLastError() + "'.");
  }
  // The DWI read is not interruptible and has no meaningful fraction; check right after it.
  context.ThrowIfCancelled();

  const int frameCount = dwiSeries->GetFrameCount();
  if (frameCount <= 0)
  {
    throw std::runtime_error("DWI series contains zero frames.");
  }

  const glm::ivec3& dimensions = dwiSeries->GetDimensions();
  if (dimensions.x <= 0 || dimensions.y <= 0 || dimensions.z <= 0)
  {
    throw std::runtime_error("DWI spatial dimensions are invalid.");
  }

  const size_t spatialVoxelCount =
    static_cast<size_t>(dimensions.x) *
    static_cast<size_t>(dimensions.y) *
    static_cast<size_t>(dimensions.z);

  const std::vector<float>& allSignals = dwiSeries->GetVoxels();
  if (allSignals.size() != spatialVoxelCount * static_cast<size_t>(frameCount))
  {
    throw std::runtime_error("DWI series voxel count does not match metadata dimensions.");
  }

  // Nothing may read allSignals/dimensions/dwiSeries after this move.
  context.dwiSeries = std::make_shared<const VolumeSeriesData>(std::move(*dwiSeries));
}

/**
 * @brief Create a Dwi Input Validation Stage object
 * 
 * @return std::unique_ptr<IMriPreprocessingStage> 
 */
std::unique_ptr<IMriPreprocessingStage> CreateDwiInputValidationStage()
{
  return std::make_unique<DWIInputValidationStage>();
}
