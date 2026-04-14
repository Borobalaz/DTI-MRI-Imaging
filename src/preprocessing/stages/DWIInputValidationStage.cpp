#include "Preprocessing/stages/DWIInputValidationStage.h"

#include <filesystem>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

#include "Preprocessing/MriPreprocessingStages.h"
#include "VolumeFileLoader.h"

namespace
{
  std::filesystem::path ResolveRequestPath(const std::string& requestedPath)
  {
    const std::filesystem::path candidate(requestedPath);
    if (candidate.is_absolute())
    {
      return candidate;
    }

    if (std::filesystem::exists(candidate))
    {
      return candidate;
    }

    return candidate;
  }
}

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
 *        If validation passes, populate the context with the resolved file paths and source volume metadata.
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

  const std::filesystem::path dwiPath = ResolveRequestPath(context.request.dwiVolumePath);
  const std::filesystem::path bvalPath = ResolveRequestPath(context.request.bvalPath);
  const std::filesystem::path bvecPath = ResolveRequestPath(context.request.bvecPath);

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

  const auto dwiSeries = VolumeFileLoader::LoadSeries(context.selectedDwiVolumePath);
  if (!dwiSeries.has_value())
  {
    throw std::runtime_error(
      "Failed to load DWI series from '" + context.selectedDwiVolumePath +
      "'. loaderError='" + VolumeFileLoader::GetLastError() + "'.");
  }

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
