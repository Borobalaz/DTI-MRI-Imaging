#include "Preprocessing/io/PreprocessingDatasetMetadataIO.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace
{
  nlohmann::json ParseParametersOrEmpty(const std::string &parametersJson)
  {
    if (parametersJson.empty())
    {
      return nlohmann::json::object();
    }

    const nlohmann::json parsed = nlohmann::json::parse(parametersJson, nullptr, false);
    return parsed.is_discarded() ? nlohmann::json::object() : parsed;
  }
}

namespace PreprocessingDatasetMetadataIO
{
  std::string Serialize(const PreprocessingDatasetMetadata &metadata)
  {
    nlohmann::json root;
    root["formatVersion"] = metadata.formatVersion;
    root["preprocessorType"] = metadata.preprocessorType;
    root["createdAtUtc"] = metadata.createdAtUtc;
    root["sourceVolumePath"] = metadata.sourceVolumePath;
    root["executedStages"] = metadata.executedStages;
    root["warnings"] = metadata.warnings;
    root["parameters"] = ParseParametersOrEmpty(metadata.parametersJson);

    nlohmann::json files = nlohmann::json::array();
    for (const PreprocessingDatasetFileEntry &entry : metadata.files)
    {
      files.push_back({{"key", entry.key}, {"relativePath", entry.relativePath}, {"kind", entry.kind}});
    }
    root["files"] = std::move(files);

    return root.dump(2);
  }

  PreprocessingDatasetMetadata Deserialize(const std::string &jsonText)
  {
    const nlohmann::json root = nlohmann::json::parse(jsonText);

    PreprocessingDatasetMetadata metadata;
    metadata.formatVersion = root.value("formatVersion", 1);
    metadata.preprocessorType = root.value("preprocessorType", std::string());
    metadata.createdAtUtc = root.value("createdAtUtc", std::string());
    metadata.sourceVolumePath = root.value("sourceVolumePath", std::string());
    metadata.executedStages = root.value("executedStages", std::vector<std::string>());
    metadata.warnings = root.value("warnings", std::vector<std::string>());
    metadata.parametersJson = root.value("parameters", nlohmann::json::object()).dump(2);

    if (root.contains("files"))
    {
      for (const nlohmann::json &entry : root.at("files"))
      {
        PreprocessingDatasetFileEntry fileEntry;
        fileEntry.key = entry.value("key", std::string());
        fileEntry.relativePath = entry.value("relativePath", std::string());
        fileEntry.kind = entry.value("kind", std::string());
        metadata.files.push_back(std::move(fileEntry));
      }
    }

    return metadata;
  }

  bool SaveToFile(const std::string &filePath, const PreprocessingDatasetMetadata &metadata)
  {
    const std::filesystem::path path(filePath);
    if (path.has_parent_path())
    {
      std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output(filePath, std::ios::binary);
    if (!output.is_open())
    {
      return false;
    }

    const std::string json = Serialize(metadata);
    output.write(json.data(), static_cast<std::streamsize>(json.size()));
    return output.good();
  }

  std::optional<PreprocessingDatasetMetadata> LoadFromFile(const std::string &filePath)
  {
    std::ifstream input(filePath, std::ios::binary);
    if (!input.is_open())
    {
      return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    try
    {
      return Deserialize(buffer.str());
    }
    catch (const std::exception &)
    {
      return std::nullopt;
    }
  }
}
