#include "Preprocessing/io/dti/DtiPreprocessingDatasetWriter.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <stdexcept>

#include "Geometry/MeshFileLoader.h"
#include "Mesh.h"
#include "Preprocessing/MriPreprocessingPipeline.h"
#include "Preprocessing/io/PreprocessingDatasetMetadataIO.h"
#include "Volume/DTIVolume.h"
#include "Volume/VolumeFileLoader.h"

namespace
{
  struct ChannelEntry
  {
    const char *name;
    VolumeData DTIVolumeChannels::*member;
  };

  const std::array<ChannelEntry, 16> &GetChannelTable()
  {
    static const std::array<ChannelEntry, 16> table = {{
        {"Dxx", &DTIVolumeChannels::Dxx}, {"Dyy", &DTIVolumeChannels::Dyy}, {"Dzz", &DTIVolumeChannels::Dzz},
        {"Dxy", &DTIVolumeChannels::Dxy}, {"Dxz", &DTIVolumeChannels::Dxz}, {"Dyz", &DTIVolumeChannels::Dyz},
        {"EVx", &DTIVolumeChannels::EVx}, {"EVy", &DTIVolumeChannels::EVy}, {"EVz", &DTIVolumeChannels::EVz},
        {"L1", &DTIVolumeChannels::L1}, {"L2", &DTIVolumeChannels::L2}, {"L3", &DTIVolumeChannels::L3},
        {"FA", &DTIVolumeChannels::FA}, {"MD", &DTIVolumeChannels::MD},
        {"AD", &DTIVolumeChannels::AD}, {"RD", &DTIVolumeChannels::RD},
    }};
    return table;
  }

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

  void SaveMeshOrThrow(const std::string &path, const Geometry &geometry)
  {
    if (!MeshFileLoader::Save(path, geometry))
    {
      throw std::runtime_error("Failed to save mesh '" + path + "': " + MeshFileLoader::GetLastError());
    }
  }
}

PreprocessingDatasetMetadata DtiPreprocessingDatasetWriter::Write(
    const std::any &result,
    const std::string &outputDirectory,
    const std::string &outputBasename,
    const std::string &parametersJson) const
{
  const MriPreprocessingResult &preprocessingResult = std::any_cast<const MriPreprocessingResult &>(result);

  const std::filesystem::path datasetDirectory = outputBasename.empty()
      ? std::filesystem::path(outputDirectory)
      : std::filesystem::path(outputDirectory) / outputBasename;

  std::filesystem::create_directories(datasetDirectory / "channels");

  PreprocessingDatasetMetadata metadata;
  metadata.formatVersion = 1;
  metadata.preprocessorType = PreprocessorType();
  metadata.createdAtUtc = CurrentUtcTimestamp();
  metadata.sourceVolumePath = preprocessingResult.report.sourceVolumePath;
  metadata.executedStages = preprocessingResult.report.executedStages;
  metadata.warnings = preprocessingResult.report.warnings;
  metadata.parametersJson = parametersJson;

  const DTIVolumeChannels &channels = preprocessingResult.dtiChannels;
  for (const ChannelEntry &channelEntry : GetChannelTable())
  {
    const std::string relativePath = std::string("channels/") + channelEntry.name + ".vxa";
    SaveVolumeOrThrow((datasetDirectory / relativePath).string(), channels.*(channelEntry.member));
    metadata.files.push_back({std::string("channel.") + channelEntry.name, relativePath, "volume"});
  }

  if (preprocessingResult.surfaceMesh && preprocessingResult.surfaceMesh->GetGeometry())
  {
    std::filesystem::create_directories(datasetDirectory / "meshes");
    const std::string relativePath = "meshes/surface.meshb";
    SaveMeshOrThrow((datasetDirectory / relativePath).string(), *preprocessingResult.surfaceMesh->GetGeometry());
    metadata.files.push_back({"mesh.surface", relativePath, "mesh"});
  }

  if (preprocessingResult.streamlineMesh && preprocessingResult.streamlineMesh->GetGeometry())
  {
    std::filesystem::create_directories(datasetDirectory / "meshes");
    const std::string relativePath = "meshes/streamlines.meshb";
    SaveMeshOrThrow((datasetDirectory / relativePath).string(), *preprocessingResult.streamlineMesh->GetGeometry());
    metadata.files.push_back({"mesh.streamlines", relativePath, "mesh"});
  }

  if (!PreprocessingDatasetMetadataIO::SaveToFile((datasetDirectory / "metadata.json").string(), metadata))
  {
    throw std::runtime_error("Failed to write metadata.json to " + datasetDirectory.string());
  }

  return metadata;
}
