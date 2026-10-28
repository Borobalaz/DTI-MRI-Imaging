#include "Preprocessing/io/fodf/FodfPreprocessingDatasetReader.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "Preprocessing/MriPreprocessingPipeline.h"
#include "Preprocessing/io/SHVolumeFileIO.h"
#include "Preprocessing/io/dti/DtiChannelTable.h"
#include "Preprocessing/io/fodf/FodfPreprocessingParameters.h"
#include "Preprocessing/io/fodf/FodfResponseJson.h"
#include "scene/volume/VolumeFileLoader.h"

std::any FodfPreprocessingDatasetReader::Read(
    const PreprocessingDatasetMetadata &metadata,
    const std::string &datasetDirectory) const
{
  if (metadata.preprocessorType != PreprocessorType())
  {
    throw std::runtime_error(
        "FodfPreprocessingDatasetReader cannot read preprocessorType '" + metadata.preprocessorType + "'.");
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

  std::shared_ptr<const VolumeData> mask;
  const auto maskEntry = relativePathByKey.find("sh.mask");
  if (maskEntry != relativePathByKey.end())
  {
    std::optional<VolumeData> loadedMask = VolumeFileLoader::Load((datasetPath / maskEntry->second).string());
    if (!loadedMask.has_value())
    {
      throw std::runtime_error("Failed to load SH mask from saved dataset.");
    }
    mask = std::make_shared<const VolumeData>(std::move(*loadedMask));
  }

  // Both SH volumes share the one loaded mask, as they do after a live run.
  const auto loadSh = [&](const std::string &relativePath) -> std::shared_ptr<const SHVolume>
  {
    SHVolume volume = SHVolumeFileIO::Load((datasetPath / relativePath).string());
    volume.mask = mask;
    if (!volume.IsWellFormed())
    {
      throw std::runtime_error("Saved SH volume '" + relativePath + "' does not match its mask dimensions.");
    }
    return std::make_shared<const SHVolume>(std::move(volume));
  };

  const auto fodfEntry = relativePathByKey.find("sh.fodf");
  if (fodfEntry == relativePathByKey.end())
  {
    throw std::runtime_error("Saved fODF dataset is missing its fODF SH volume.");
  }
  result.fodfShVolume = loadSh(fodfEntry->second);

  const auto tensorOdfEntry = relativePathByKey.find("sh.tensorOdf");
  if (tensorOdfEntry != relativePathByKey.end())
  {
    result.shVolume = loadSh(tensorOdfEntry->second);
  }

  const auto responseEntry = relativePathByKey.find("response");
  if (responseEntry == relativePathByKey.end())
  {
    throw std::runtime_error("Saved fODF dataset is missing its response function.");
  }
  {
    const std::filesystem::path responsePath = datasetPath / responseEntry->second;
    std::ifstream input(responsePath);
    if (!input)
    {
      throw std::runtime_error("Failed to open " + responsePath.string());
    }
    const std::string responseText((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    result.fodfResponse = DeserializeFodfResponseJson(responseText);
  }

  result.fodfIgnoredShellBValues = ReadFodfIgnoredShellBValues(metadata.parametersJson);

  result.report.sourceVolumePath = metadata.sourceVolumePath;
  result.report.executedStages = metadata.executedStages;
  result.report.warnings = metadata.warnings;

  return result;
}
