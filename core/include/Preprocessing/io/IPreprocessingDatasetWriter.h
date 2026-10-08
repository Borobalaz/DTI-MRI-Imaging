#pragma once

#include <any>
#include <string>

#include "Preprocessing/io/PreprocessingDatasetMetadata.h"

/**
 * @brief Writes one preprocessor's result to disk as a saved dataset (payload files + a
 *        metadata.json manifest), dispatched by PreprocessingDatasetFormatRegistry on
 *        PreprocessorType(). `result` must hold this writer's known concrete result type
 *        (e.g. MriPreprocessingResult for the DTI writer) - the registry and this interface
 *        stay agnostic to that type via std::any, so adding a new preprocessor's writer never
 *        requires changing this interface or the registry.
 */
class IPreprocessingDatasetWriter
{
public:
  virtual ~IPreprocessingDatasetWriter() = default;

  virtual std::string PreprocessorType() const = 0;

  virtual PreprocessingDatasetMetadata Write(
      const std::any &result,
      const std::string &outputDirectory,
      const std::string &outputBasename,
      const std::string &parametersJson) const = 0;
};
