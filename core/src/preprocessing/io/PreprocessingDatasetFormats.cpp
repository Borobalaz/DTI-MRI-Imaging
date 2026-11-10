#include "Preprocessing/io/PreprocessingDatasetFormats.h"

#include <memory>

#include "Preprocessing/io/dmri/DmriPreprocessingDatasetReader.h"
#include "Preprocessing/io/dmri/DmriPreprocessingDatasetWriter.h"
#include "Preprocessing/io/dti/DtiPreprocessingDatasetReader.h"
#include "Preprocessing/io/fodf/FodfPreprocessingDatasetReader.h"

PreprocessingDatasetFormatRegistry &GetDefaultPreprocessingDatasetFormatRegistry()
{
  static PreprocessingDatasetFormatRegistry registry = []
  {
    PreprocessingDatasetFormatRegistry formatRegistry;
    formatRegistry.RegisterWriter(std::make_shared<DmriPreprocessingDatasetWriter>());
    formatRegistry.RegisterReader(std::make_shared<DmriPreprocessingDatasetReader>());
    // Legacy formats are read-only: datasets saved before the unified dmri format still load.
    formatRegistry.RegisterReader(std::make_shared<DtiPreprocessingDatasetReader>());
    formatRegistry.RegisterReader(std::make_shared<FodfPreprocessingDatasetReader>());
    return formatRegistry;
  }();

  return registry;
}
