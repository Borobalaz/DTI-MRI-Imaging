#pragma once

#include "Preprocessing/io/IPreprocessingDatasetReader.h"

/**
 * @brief Reconstructs an MriPreprocessingResult from a saved "dmri" dataset, ready for
 *        DtiVolumeScene::ApplyPreprocessingResult - the same path a live run uses. Optional
 *        payloads that are absent stay empty; fodf/status.json restores fodfUnavailableReason and
 *        the absolute amplitude cutoff.
 */
class DmriPreprocessingDatasetReader : public IPreprocessingDatasetReader
{
public:
  std::string PreprocessorType() const override;

  std::any Read(
      const PreprocessingDatasetMetadata &metadata,
      const std::string &datasetDirectory) const override;
};
