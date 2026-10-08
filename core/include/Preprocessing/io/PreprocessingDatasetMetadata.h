#pragma once

#include <string>
#include <vector>

struct PreprocessingDatasetFileEntry
{
  std::string key;          // model-defined id, e.g. "channel.FA", "mesh.surface" - opaque to the registry
  std::string relativePath; // relative to the dataset directory, e.g. "channels/FA.vxa"
  std::string kind;         // free-form, e.g. "volume" / "mesh" - informational only
};

/**
 * @brief Generic, model-agnostic envelope describing a saved preprocessing dataset: which
 *        preprocessor produced it (preprocessorType dispatches to the matching reader/writer,
 *        see PreprocessingDatasetFormatRegistry), basic provenance, and a manifest of payload
 *        files. Model-specific provenance (source paths, tuning parameters) lives opaquely in
 *        parametersJson, so this struct never needs to change when a new preprocessor model is
 *        added.
 */
struct PreprocessingDatasetMetadata
{
  int formatVersion = 1;
  std::string preprocessorType;
  std::string createdAtUtc;
  std::string sourceVolumePath;
  std::vector<std::string> executedStages;
  std::vector<std::string> warnings;
  std::string parametersJson;
  std::vector<PreprocessingDatasetFileEntry> files;
};
