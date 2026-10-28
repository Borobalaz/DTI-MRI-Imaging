#pragma once

#include "Preprocessing/io/IPreprocessingDatasetReader.h"

/**
 * @brief Reconstructs an fODF MriPreprocessingResult (dtiChannels, fodfShVolume, optional
 *        shVolume, fodfResponse, fodfIgnoredShellBValues) from a saved "fodf" dataset, ready for
 *        DtiVolumeScene::ApplyPreprocessingResult - the same path a live run uses.
 */
class FodfPreprocessingDatasetReader : public IPreprocessingDatasetReader
{
public:
  std::string PreprocessorType() const override { return "fodf"; }

  std::any Read(
      const PreprocessingDatasetMetadata &metadata,
      const std::string &datasetDirectory) const override;
};
