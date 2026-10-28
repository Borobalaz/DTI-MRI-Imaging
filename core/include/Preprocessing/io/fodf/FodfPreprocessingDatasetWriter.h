#pragma once

#include "Preprocessing/io/IPreprocessingDatasetWriter.h"

/**
 * @brief Saves an fODF MriPreprocessingResult to disk as metadata.json + channels/*.vxa (the same
 *        16 DTI channels as the "dti" format) + sh/fodf.shb + sh/tensor_odf.shb (optional) +
 *        sh/mask.vxa (optional) + response.json.
 */
class FodfPreprocessingDatasetWriter : public IPreprocessingDatasetWriter
{
public:
  std::string PreprocessorType() const override { return "fodf"; }

  PreprocessingDatasetMetadata Write(
      const std::any &result,
      const std::string &outputDirectory,
      const std::string &outputBasename,
      const std::string &parametersJson) const override;
};
