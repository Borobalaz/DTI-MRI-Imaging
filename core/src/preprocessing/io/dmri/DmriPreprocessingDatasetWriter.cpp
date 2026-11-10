#include "Preprocessing/io/dmri/DmriPreprocessingDatasetWriter.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "Preprocessing/MriPreprocessingPipeline.h"
#include "Preprocessing/io/PreprocessingDatasetMetadataIO.h"
#include "Preprocessing/io/SHVolumeFileIO.h"
#include "Preprocessing/io/dmri/DmriDatasetJson.h"
#include "Preprocessing/io/dmri/DmriDatasetLayout.h"
#include "Preprocessing/io/dti/DtiChannelTable.h"
#include "Preprocessing/io/fodf/FodfResponseJson.h"
#include "scene/geometry/MeshFileLoader.h"
#include "scene/object/Mesh.h"
#include "scene/volume/FodfVolume.h"
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

  // Writes one payload file under the dataset directory and records it in the manifest.
  class DatasetPayloadWriter
  {
  public:
    DatasetPayloadWriter(std::filesystem::path datasetDirectory, PreprocessingDatasetMetadata &metadata)
        : datasetDirectory(std::move(datasetDirectory)), metadata(metadata)
    {
    }

    void WriteVolume(const std::string &key, const std::string &relativePath, const VolumeData &volume)
    {
      if (!VolumeFileLoader::Save(PrepareTarget(relativePath).string(), volume))
      {
        throw std::runtime_error("Failed to save volume: " + (datasetDirectory / relativePath).string());
      }
      Record(key, relativePath, "volume");
    }

    void WriteMesh(const std::string &key, const std::string &relativePath, const Geometry &geometry)
    {
      if (!MeshFileLoader::Save(PrepareTarget(relativePath).string(), geometry))
      {
        throw std::runtime_error("Failed to save mesh '" + (datasetDirectory / relativePath).string() +
                                 "': " + MeshFileLoader::GetLastError());
      }
      Record(key, relativePath, "mesh");
    }

    void WriteSh(const std::string &key, const std::string &relativePath, const SHVolume &volume)
    {
      SHVolumeFileIO::Save(PrepareTarget(relativePath).string(), volume);
      Record(key, relativePath, "sh");
    }

    void WriteJson(const std::string &key, const std::string &relativePath, const std::string &jsonText)
    {
      const std::filesystem::path target = PrepareTarget(relativePath);
      std::ofstream output(target, std::ios::trunc);
      output << jsonText;
      output.flush();
      if (!output.good())
      {
        throw std::runtime_error("Failed to write " + target.string());
      }
      Record(key, relativePath, "json");
    }

  private:
    std::filesystem::path PrepareTarget(const std::string &relativePath) const
    {
      const std::filesystem::path target = datasetDirectory / relativePath;
      std::filesystem::create_directories(target.parent_path());
      return target;
    }

    void Record(const std::string &key, const std::string &relativePath, const std::string &kind)
    {
      metadata.files.push_back({key, relativePath, kind});
    }

    std::filesystem::path datasetDirectory;
    PreprocessingDatasetMetadata &metadata;
  };

  void WriteDtiChannels(DatasetPayloadWriter &writer, const DTIVolumeChannels &channels)
  {
    for (const DtiChannelEntry &channelEntry : GetDtiChannelTable())
    {
      writer.WriteVolume(std::string(DmriDatasetLayout::kChannelKeyPrefix) + channelEntry.name,
                         std::string(DmriDatasetLayout::kChannelDirectory) + channelEntry.name +
                             DmriDatasetLayout::kVolumeExtension,
                         channels.*(channelEntry.member));
    }
  }

  void WriteFodfMaps(DatasetPayloadWriter &writer, const FodfVolumeChannels &maps)
  {
    for (const DmriDatasetLayout::FodfMapChannelEntry &mapEntry : DmriDatasetLayout::kFodfMapChannelTable)
    {
      writer.WriteVolume(std::string(DmriDatasetLayout::kFodfMapKeyPrefix) + mapEntry.name,
                         std::string(DmriDatasetLayout::kFodfMapDirectory) + mapEntry.name +
                             DmriDatasetLayout::kVolumeExtension,
                         maps.*(mapEntry.member));
    }
  }

  void WriteShVolumes(DatasetPayloadWriter &writer, const MriPreprocessingResult &result)
  {
    if (result.shVolume)
    {
      writer.WriteSh(DmriDatasetLayout::kTensorOdfShKey, DmriDatasetLayout::kTensorOdfShPath, *result.shVolume);
    }
    if (result.fodfShVolume)
    {
      writer.WriteSh(DmriDatasetLayout::kFodfShKey, DmriDatasetLayout::kFodfShPath, *result.fodfShVolume);
    }

    // Both SH volumes share the raw-FA mask on a live run; the .shb files do not carry it.
    std::shared_ptr<const VolumeData> mask = result.fodfShVolume ? result.fodfShVolume->mask : nullptr;
    if (!mask && result.shVolume)
    {
      mask = result.shVolume->mask;
    }
    if (mask)
    {
      writer.WriteVolume(DmriDatasetLayout::kShMaskKey, DmriDatasetLayout::kShMaskPath, *mask);
    }
  }

  void WriteMeshIfPresent(DatasetPayloadWriter &writer,
                          const std::shared_ptr<Mesh> &mesh,
                          const char *key,
                          const char *relativePath)
  {
    if (mesh && mesh->GetGeometry())
    {
      writer.WriteMesh(key, relativePath, *mesh->GetGeometry());
    }
  }

  DmriFodfStatus BuildFodfStatus(const MriPreprocessingResult &result)
  {
    DmriFodfStatus status;
    status.available = result.fodfUnavailableReason.empty() && result.fodfShVolume && result.fodfMaps;
    status.unavailableReason = result.fodfUnavailableReason;
    status.absoluteAmplitudeCutoff = result.fodfAbsoluteAmplitudeCutoff;
    return status;
  }
}

std::string DmriPreprocessingDatasetWriter::PreprocessorType() const
{
  return DmriDatasetLayout::kPreprocessorType;
}

PreprocessingDatasetMetadata DmriPreprocessingDatasetWriter::Write(
    const std::any &result,
    const std::string &outputDirectory,
    const std::string &outputBasename,
    const std::string &parametersJson) const
{
  const MriPreprocessingResult &preprocessingResult = std::any_cast<const MriPreprocessingResult &>(result);

  const std::filesystem::path datasetDirectory = outputBasename.empty()
      ? std::filesystem::path(outputDirectory)
      : std::filesystem::path(outputDirectory) / outputBasename;
  std::filesystem::create_directories(datasetDirectory);

  PreprocessingDatasetMetadata metadata;
  metadata.formatVersion = DmriDatasetLayout::kFormatVersion;
  metadata.preprocessorType = PreprocessorType();
  metadata.createdAtUtc = CurrentUtcTimestamp();
  metadata.sourceVolumePath = preprocessingResult.report.sourceVolumePath;
  metadata.executedStages = preprocessingResult.report.executedStages;
  metadata.warnings = preprocessingResult.report.warnings;
  metadata.parametersJson = parametersJson;

  DatasetPayloadWriter writer(datasetDirectory, metadata);

  WriteDtiChannels(writer, preprocessingResult.dtiChannels);
  if (preprocessingResult.fodfMaps)
  {
    WriteFodfMaps(writer, *preprocessingResult.fodfMaps);
  }
  WriteShVolumes(writer, preprocessingResult);
  if (preprocessingResult.fodfResponse.IsValid())
  {
    writer.WriteJson(DmriDatasetLayout::kResponseKey, DmriDatasetLayout::kResponsePath,
                     SerializeFodfResponseJson(preprocessingResult.fodfResponse));
  }

  WriteMeshIfPresent(writer, preprocessingResult.surfaceMesh,
                     DmriDatasetLayout::kSurfaceMeshKey, DmriDatasetLayout::kSurfaceMeshPath);
  WriteMeshIfPresent(writer, preprocessingResult.streamlineMesh,
                     DmriDatasetLayout::kDtiStreamlineMeshKey, DmriDatasetLayout::kDtiStreamlineMeshPath);
  WriteMeshIfPresent(writer, preprocessingResult.fodfStreamlineMesh,
                     DmriDatasetLayout::kFodfStreamlineMeshKey, DmriDatasetLayout::kFodfStreamlineMeshPath);

  if (!preprocessingResult.seedPoints.empty())
  {
    writer.WriteJson(DmriDatasetLayout::kSeedPointsKey, DmriDatasetLayout::kSeedPointsPath,
                     SerializeSeedPointsJson(preprocessingResult.seedPoints));
  }

  writer.WriteJson(DmriDatasetLayout::kFodfStatusKey, DmriDatasetLayout::kFodfStatusPath,
                   SerializeDmriFodfStatusJson(BuildFodfStatus(preprocessingResult)));

  if (!PreprocessingDatasetMetadataIO::SaveToFile((datasetDirectory / "metadata.json").string(), metadata))
  {
    throw std::runtime_error("Failed to write metadata.json to " + datasetDirectory.string());
  }

  return metadata;
}
