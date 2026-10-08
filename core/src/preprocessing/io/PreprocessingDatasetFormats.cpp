#include "Preprocessing/io/PreprocessingDatasetFormats.h"

#include <memory>

#include "Preprocessing/io/dti/DtiPreprocessingDatasetReader.h"
#include "Preprocessing/io/dti/DtiPreprocessingDatasetWriter.h"

PreprocessingDatasetFormatRegistry &GetDefaultPreprocessingDatasetFormatRegistry()
{
  static PreprocessingDatasetFormatRegistry registry = []
  {
    PreprocessingDatasetFormatRegistry formatRegistry;
    formatRegistry.RegisterWriter(std::make_shared<DtiPreprocessingDatasetWriter>());
    formatRegistry.RegisterReader(std::make_shared<DtiPreprocessingDatasetReader>());
    return formatRegistry;
  }();

  return registry;
}
