#include "Preprocessing/io/PreprocessingDatasetFormats.h"

#include <memory>

#include "Preprocessing/io/dti/DtiPreprocessingDatasetReader.h"
#include "Preprocessing/io/dti/DtiPreprocessingDatasetWriter.h"
#include "Preprocessing/io/fodf/FodfPreprocessingDatasetReader.h"
#include "Preprocessing/io/fodf/FodfPreprocessingDatasetWriter.h"

PreprocessingDatasetFormatRegistry &GetDefaultPreprocessingDatasetFormatRegistry()
{
  static PreprocessingDatasetFormatRegistry registry = []
  {
    PreprocessingDatasetFormatRegistry formatRegistry;
    formatRegistry.RegisterWriter(std::make_shared<DtiPreprocessingDatasetWriter>());
    formatRegistry.RegisterReader(std::make_shared<DtiPreprocessingDatasetReader>());
    formatRegistry.RegisterWriter(std::make_shared<FodfPreprocessingDatasetWriter>());
    formatRegistry.RegisterReader(std::make_shared<FodfPreprocessingDatasetReader>());
    return formatRegistry;
  }();

  return registry;
}
