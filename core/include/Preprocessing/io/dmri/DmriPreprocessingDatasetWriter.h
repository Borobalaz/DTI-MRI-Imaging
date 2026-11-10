#pragma once

#include "Preprocessing/io/IPreprocessingDatasetWriter.h"

/**
 * @brief Saves an MriToDmriPreprocessor result (or any MriPreprocessingResult) as a "dmri"
 *        dataset: metadata.json + the 16 DTI channels, plus whatever optional outputs the result
 *        holds - the 14 fODF maps, the tensor-ODF/fODF SH volumes and their mask, the response,
 *        the surface, DTI and fODF streamline meshes and the seed points - and always
 *        fodf/status.json. See DmriDatasetLayout.h for the keys and paths.
 */
class DmriPreprocessingDatasetWriter : public IPreprocessingDatasetWriter
{
public:
  std::string PreprocessorType() const override;

  PreprocessingDatasetMetadata Write(
      const std::any &result,
      const std::string &outputDirectory,
      const std::string &outputBasename,
      const std::string &parametersJson) const override;
};
