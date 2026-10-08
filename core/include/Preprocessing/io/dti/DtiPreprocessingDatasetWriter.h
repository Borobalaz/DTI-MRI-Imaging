#pragma once

#include "Preprocessing/io/IPreprocessingDatasetWriter.h"

/**
 * @brief Saves an MriPreprocessingResult (DTI's 17 volume channels + optional surface/
 *        streamline meshes) to disk as metadata.json + channels/*.vxa + meshes/*.meshb.
 */
class DtiPreprocessingDatasetWriter : public IPreprocessingDatasetWriter
{
public:
  std::string PreprocessorType() const override { return "dti"; }

  PreprocessingDatasetMetadata Write(
      const std::any &result,
      const std::string &outputDirectory,
      const std::string &outputBasename,
      const std::string &parametersJson) const override;
};
