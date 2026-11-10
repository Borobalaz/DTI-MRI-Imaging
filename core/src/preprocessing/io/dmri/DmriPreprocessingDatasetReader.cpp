#include "Preprocessing/io/dmri/DmriPreprocessingDatasetReader.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include "Preprocessing/MriPreprocessingPipeline.h"
#include "Preprocessing/io/SHVolumeFileIO.h"
#include "Preprocessing/io/dmri/DmriDatasetJson.h"
#include "Preprocessing/io/dmri/DmriDatasetLayout.h"
#include "Preprocessing/io/dti/DtiChannelTable.h"
#include "Preprocessing/io/fodf/FodfPreprocessingParameters.h"
#include "Preprocessing/io/fodf/FodfResponseJson.h"
#include "scene/geometry/MeshFileLoader.h"
#include "scene/object/Mesh.h"
#include "scene/volume/FodfVolume.h"
#include "scene/volume/VolumeFileLoader.h"

namespace
{
  // Resolves manifest keys to payload files of one dataset directory.
  class DatasetPayloadReader
  {
  public:
    DatasetPayloadReader(const PreprocessingDatasetMetadata &metadata, const std::string &datasetDirectory)
        : datasetPath(datasetDirectory)
    {
      for (const PreprocessingDatasetFileEntry &entry : metadata.files)
      {
        relativePathByKey[entry.key] = entry.relativePath;
      }
    }

    bool Has(const std::string &key) const { return relativePathByKey.count(key) != 0; }

    VolumeData ReadVolume(const std::string &key) const
    {
      std::optional<VolumeData> volume = VolumeFileLoader::Load(PathOf(key).string());
      if (!volume.has_value())
      {
        throw std::runtime_error("Failed to load volume '" + key + "' from saved dataset.");
      }
      return std::move(*volume);
    }

    std::shared_ptr<Mesh> ReadMeshIfPresent(const std::string &key) const
    {
      if (!Has(key))
      {
        return nullptr;
      }
      std::shared_ptr<Geometry> geometry = MeshFileLoader::Load(PathOf(key).string());
      if (!geometry)
      {
        throw std::runtime_error("Failed to load mesh '" + key + "' from saved dataset: " +
                                 MeshFileLoader::GetLastError());
      }
      return std::make_shared<Mesh>(geometry, nullptr);
    }

    SHVolume ReadSh(const std::string &key) const
    {
      return SHVolumeFileIO::Load(PathOf(key).string());
    }

    std::string ReadText(const std::string &key) const
    {
      const std::filesystem::path path = PathOf(key);
      std::ifstream input(path);
      if (!input)
      {
        throw std::runtime_error("Failed to open " + path.string());
      }
      return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    }

  private:
    std::filesystem::path PathOf(const std::string &key) const
    {
      const auto found = relativePathByKey.find(key);
      if (found == relativePathByKey.end())
      {
        throw std::runtime_error("Saved dataset is missing '" + key + "'.");
      }
      return datasetPath / found->second;
    }

    std::filesystem::path datasetPath;
    std::unordered_map<std::string, std::string> relativePathByKey;
  };

  void ReadDtiChannels(const DatasetPayloadReader &reader, DTIVolumeChannels &channels)
  {
    for (const DtiChannelEntry &channelEntry : GetDtiChannelTable())
    {
      channels.*(channelEntry.member) =
          reader.ReadVolume(std::string(DmriDatasetLayout::kChannelKeyPrefix) + channelEntry.name);
    }
  }

  // All 14 maps or none; a partial set means a damaged dataset.
  std::shared_ptr<const FodfVolumeChannels> ReadFodfMaps(const DatasetPayloadReader &reader)
  {
    size_t presentCount = 0;
    for (const DmriDatasetLayout::FodfMapChannelEntry &mapEntry : DmriDatasetLayout::kFodfMapChannelTable)
    {
      presentCount += reader.Has(std::string(DmriDatasetLayout::kFodfMapKeyPrefix) + mapEntry.name) ? 1U : 0U;
    }
    if (presentCount == 0)
    {
      return nullptr;
    }
    if (presentCount != DmriDatasetLayout::kFodfMapChannelTable.size())
    {
      throw std::runtime_error("Saved dataset has an incomplete set of fODF maps.");
    }

    auto maps = std::make_shared<FodfVolumeChannels>();
    for (const DmriDatasetLayout::FodfMapChannelEntry &mapEntry : DmriDatasetLayout::kFodfMapChannelTable)
    {
      (*maps).*(mapEntry.member) =
          reader.ReadVolume(std::string(DmriDatasetLayout::kFodfMapKeyPrefix) + mapEntry.name);
    }
    return maps;
  }

  void ReadShVolumes(const DatasetPayloadReader &reader, MriPreprocessingResult &result)
  {
    std::shared_ptr<const VolumeData> mask;
    if (reader.Has(DmriDatasetLayout::kShMaskKey))
    {
      mask = std::make_shared<const VolumeData>(reader.ReadVolume(DmriDatasetLayout::kShMaskKey));
    }

    // Both SH volumes share the one loaded mask, as they do after a live run.
    const auto readShIfPresent = [&](const char *key) -> std::shared_ptr<const SHVolume>
    {
      if (!reader.Has(key))
      {
        return nullptr;
      }
      SHVolume volume = reader.ReadSh(key);
      volume.mask = mask;
      if (!volume.IsWellFormed())
      {
        throw std::runtime_error("Saved SH volume '" + std::string(key) + "' does not match its mask dimensions.");
      }
      return std::make_shared<const SHVolume>(std::move(volume));
    };

    result.shVolume = readShIfPresent(DmriDatasetLayout::kTensorOdfShKey);
    result.fodfShVolume = readShIfPresent(DmriDatasetLayout::kFodfShKey);
  }
}

std::string DmriPreprocessingDatasetReader::PreprocessorType() const
{
  return DmriDatasetLayout::kPreprocessorType;
}

std::any DmriPreprocessingDatasetReader::Read(
    const PreprocessingDatasetMetadata &metadata,
    const std::string &datasetDirectory) const
{
  if (metadata.preprocessorType != PreprocessorType())
  {
    throw std::runtime_error(
        "DmriPreprocessingDatasetReader cannot read preprocessorType '" + metadata.preprocessorType + "'.");
  }
  if (metadata.formatVersion != DmriDatasetLayout::kFormatVersion)
  {
    throw std::runtime_error("Unsupported dmri dataset formatVersion " + std::to_string(metadata.formatVersion) + ".");
  }

  const DatasetPayloadReader reader(metadata, datasetDirectory);

  MriPreprocessingResult result;
  ReadDtiChannels(reader, result.dtiChannels);
  result.fodfMaps = ReadFodfMaps(reader);
  ReadShVolumes(reader, result);

  if (reader.Has(DmriDatasetLayout::kResponseKey))
  {
    result.fodfResponse = DeserializeFodfResponseJson(reader.ReadText(DmriDatasetLayout::kResponseKey));
  }

  result.surfaceMesh = reader.ReadMeshIfPresent(DmriDatasetLayout::kSurfaceMeshKey);
  result.streamlineMesh = reader.ReadMeshIfPresent(DmriDatasetLayout::kDtiStreamlineMeshKey);
  result.fodfStreamlineMesh = reader.ReadMeshIfPresent(DmriDatasetLayout::kFodfStreamlineMeshKey);

  if (reader.Has(DmriDatasetLayout::kSeedPointsKey))
  {
    result.seedPoints = DeserializeSeedPointsJson(reader.ReadText(DmriDatasetLayout::kSeedPointsKey));
  }

  const DmriFodfStatus status = DeserializeDmriFodfStatusJson(reader.ReadText(DmriDatasetLayout::kFodfStatusKey));
  result.fodfUnavailableReason = status.unavailableReason;
  result.fodfAbsoluteAmplitudeCutoff = status.absoluteAmplitudeCutoff;

  result.fodfIgnoredShellBValues = ReadFodfIgnoredShellBValues(metadata.parametersJson);

  result.report.sourceVolumePath = metadata.sourceVolumePath;
  result.report.executedStages = metadata.executedStages;
  result.report.warnings = metadata.warnings;

  return result;
}
