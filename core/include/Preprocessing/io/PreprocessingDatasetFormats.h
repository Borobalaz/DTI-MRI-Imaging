#pragma once

#include "Preprocessing/io/PreprocessingDatasetFormatRegistry.h"

/**
 * @brief Composition root for saved-dataset formats. To add a new preprocessor's saved-dataset
 *        support (e.g. a future fODF model), implement IPreprocessingDatasetWriter/Reader for
 *        it and register both inside this function's body - no other file under
 *        Preprocessing/io needs to change.
 */
PreprocessingDatasetFormatRegistry &GetDefaultPreprocessingDatasetFormatRegistry();
