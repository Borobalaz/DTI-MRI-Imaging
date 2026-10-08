#pragma once

#include <optional>
#include <string>

#include "Preprocessing/io/PreprocessingDatasetMetadata.h"

/**
 * @brief JSON (de)serialization for PreprocessingDatasetMetadata (metadata.json). This is the
 *        only place that needs to know the on-disk JSON shape of the envelope.
 */
namespace PreprocessingDatasetMetadataIO
{
  std::string Serialize(const PreprocessingDatasetMetadata &metadata);
  PreprocessingDatasetMetadata Deserialize(const std::string &jsonText);

  bool SaveToFile(const std::string &filePath, const PreprocessingDatasetMetadata &metadata);
  std::optional<PreprocessingDatasetMetadata> LoadFromFile(const std::string &filePath);
}
