#include "Preprocessing/io/dti/DtiPreprocessingDatasetReader.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include "scene/geometry/MeshFileLoader.h"
#include "scene/object/Mesh.h"
#include "Preprocessing/MriPreprocessingPipeline.h"
#include "Preprocessing/io/dti/DtiChannelTable.h"
#include "scene/volume/DTIVolume.h"
#include "scene/volume/VolumeFileLoader.h"

std::any DtiPreprocessingDatasetReader::Read(
    const PreprocessingDatasetMetadata &metadata,
    const std::string &datasetDirectory) const
{
  if (metadata.preprocessorType != PreprocessorType())
  {
    throw std::runtime_error(
        "DtiPreprocessingDatasetReader cannot read preprocessorType '" + metadata.preprocessorType + "'.");
  }

  std::unordered_map<std::string, std::string> relativePathByKey;
  for (const PreprocessingDatasetFileEntry &entry : metadata.files)
  {
    relativePathByKey[entry.key] = entry.relativePath;
  }

  const std::filesystem::path datasetPath(datasetDirectory);

  MriPreprocessingResult result;
  for (const DtiChannelEntry &channelEntry : GetDtiChannelTable())
  {
    const std::string key = std::string("channel.") + channelEntry.name;
    const auto found = relativePathByKey.find(key);
    if (found == relativePathByKey.end())
    {
      throw std::runtime_error("Saved dataset is missing channel '" + std::string(channelEntry.name) + "'.");
    }

    const std::optional<VolumeData> loadedVolume = VolumeFileLoader::Load((datasetPath / found->second).string());
    if (!loadedVolume.has_value())
    {
      throw std::runtime_error("Failed to load channel '" + std::string(channelEntry.name) + "' from saved dataset.");
    }

    result.dtiChannels.*(channelEntry.member) = *loadedVolume;
  }

  const auto surfaceEntry = relativePathByKey.find("mesh.surface");
  if (surfaceEntry != relativePathByKey.end())
  {
    std::shared_ptr<Geometry> geometry = MeshFileLoader::Load((datasetPath / surfaceEntry->second).string());
    if (!geometry)
    {
      throw std::runtime_error("Failed to load surface mesh from saved dataset: " + MeshFileLoader::GetLastError());
    }
    result.surfaceMesh = std::make_shared<Mesh>(geometry, nullptr);
  }

  const auto streamlineEntry = relativePathByKey.find("mesh.streamlines");
  if (streamlineEntry != relativePathByKey.end())
  {
    std::shared_ptr<Geometry> geometry = MeshFileLoader::Load((datasetPath / streamlineEntry->second).string());
    if (!geometry)
    {
      throw std::runtime_error(
          "Failed to load streamline mesh from saved dataset: " + MeshFileLoader::GetLastError());
    }
    result.streamlineMesh = std::make_shared<Mesh>(geometry, nullptr);
  }

  result.report.sourceVolumePath = metadata.sourceVolumePath;
  result.report.executedStages = metadata.executedStages;
  result.report.warnings = metadata.warnings;

  return result;
}
