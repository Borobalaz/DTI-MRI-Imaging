#pragma once

#include <any>
#include <string>

#include "Preprocessing/io/PreprocessingDatasetMetadata.h"

/**
 * @brief Reads a saved dataset back from disk into its original result type, dispatched by
 *        PreprocessingDatasetFormatRegistry on metadata.preprocessorType. Returns the result
 *        wrapped in std::any (e.g. an MriPreprocessingResult for the DTI reader) so this
 *        interface and the registry never need to know about any concrete result shape.
 */
class IPreprocessingDatasetReader
{
public:
  virtual ~IPreprocessingDatasetReader() = default;

  virtual std::string PreprocessorType() const = 0;

  virtual std::any Read(
      const PreprocessingDatasetMetadata &metadata,
      const std::string &datasetDirectory) const = 0;
};
