#include "Preprocessing/io/fodf/FodfPreprocessingDatasetWriter.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "Preprocessing/MriPreprocessingPipeline.h"
#include "Preprocessing/io/PreprocessingDatasetMetadataIO.h"
#include "Preprocessing/io/SHVolumeFileIO.h"
#include "Preprocessing/io/dti/DtiChannelTable.h"
#include "Preprocessing/io/fodf/FodfResponseJson.h"
#include "scene/volume/VolumeFileLoader.h"

namespace
{
  std::string CurrentUtcTimestamp()
  {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utcTime{};
#if defined(_WIN32)
    gmtime_s(&utcTime, &now);
#else
    gmtime_r(&now, &utcTime);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utcTime);
    return std::string(buffer);
  }

  void SaveVolumeOrThrow(const std::string &path, const VolumeData &volume)
  {
    if (!VolumeFileLoader::Save(path, volume))
    {
      throw std::runtime_error("Failed to save preprocessed channel: " + path);
    }
  }
}

PreprocessingDatasetMetadata FodfPreprocessingDatasetWriter::Write(
    const std::any &result,
    const std::string &outputDirectory,
    const std::string &outputBasename,
    const std::string &parametersJson) const
{
  const MriPreprocessingResult &preprocessingResult = std::any_cast<const MriPreprocessingResult &>(result);
  if (!preprocessingResult.fodfShVolume)
  {
    throw std::runtime_error("fODF dataset writer requires an fODF SH volume");
  }
  if (!preprocessingResult.fodfResponse.IsValid())
  {
    throw std::runtime_error("fODF dataset writer requires a valid response function");
  }

  const std::filesystem::path datasetDirectory = outputBasename.empty()
      ? std::filesystem::path(outputDirectory)
      : std::filesystem::path(outputDirectory) / outputBasename;

  std::filesystem::create_directories(datasetDirectory / "channels");
  std::filesystem::create_directories(datasetDirectory / "sh");

  PreprocessingDatasetMetadata metadata;
  metadata.formatVersion = 1;
  metadata.preprocessorType = PreprocessorType();
  metadata.createdAtUtc = CurrentUtcTimestamp();
  metadata.sourceVolumePath = preprocessingResult.report.sourceVolumePath;
  metadata.executedStages = preprocessingResult.report.executedStages;
  metadata.warnings = preprocessingResult.report.warnings;
  metadata.parametersJson = parametersJson;

  const DTIVolumeChannels &channels = preprocessingResult.dtiChannels;
  for (const DtiChannelEntry &channelEntry : GetDtiChannelTable())
  {
    const std::string relativePath = std::string("channels/") + channelEntry.name + ".vxa";
    SaveVolumeOrThrow((datasetDirectory / relativePath).string(), channels.*(channelEntry.member));
    metadata.files.push_back({std::string("channel.") + channelEntry.name, relativePath, "volume"});
  }

  {
    const std::string relativePath = "sh/fodf.shb";
    SHVolumeFileIO::Save((datasetDirectory / relativePath).string(), *preprocessingResult.fodfShVolume);
    metadata.files.push_back({"sh.fodf", relativePath, "sh"});
  }

  if (preprocessingResult.shVolume)
  {
    const std::string relativePath = "sh/tensor_odf.shb";
    SHVolumeFileIO::Save((datasetDirectory / relativePath).string(), *preprocessingResult.shVolume);
    metadata.files.push_back({"sh.tensorOdf", relativePath, "sh"});
  }

  // Both SH volumes share the raw-FA mask on a live run; fall back to the tensor ODF's mask.
  std::shared_ptr<const VolumeData> mask = preprocessingResult.fodfShVolume->mask;
  if (!mask && preprocessingResult.shVolume)
  {
    mask = preprocessingResult.shVolume->mask;
  }
  if (mask)
  {
    const std::string relativePath = "sh/mask.vxa";
    SaveVolumeOrThrow((datasetDirectory / relativePath).string(), *mask);
    metadata.files.push_back({"sh.mask", relativePath, "volume"});
  }

  {
    const std::string relativePath = "response.json";
    std::ofstream output(datasetDirectory / relativePath, std::ios::trunc);
    output << SerializeFodfResponseJson(preprocessingResult.fodfResponse);
    output.flush();
    if (!output.good())
    {
      throw std::runtime_error("Failed to write " + (datasetDirectory / relativePath).string());
    }
    metadata.files.push_back({"response", relativePath, "json"});
  }

  if (!PreprocessingDatasetMetadataIO::SaveToFile((datasetDirectory / "metadata.json").string(), metadata))
  {
    throw std::runtime_error("Failed to write metadata.json to " + datasetDirectory.string());
  }

  return metadata;
}
