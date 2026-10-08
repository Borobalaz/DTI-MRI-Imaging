#pragma once

#include "Preprocessing/io/IPreprocessingDatasetReader.h"

/**
 * @brief Reconstructs an MriPreprocessingResult from a saved DTI dataset (metadata.json +
 *        channels/*.vxa + meshes/*.meshb), ready to feed into
 *        DtiVolumeScene::ApplyPreprocessingResult - the same function a live preprocessing run
 *        uses, which is what guarantees a loaded dataset produces identical scene output to a
 *        live run.
 */
class DtiPreprocessingDatasetReader : public IPreprocessingDatasetReader
{
public:
  std::string PreprocessorType() const override { return "dti"; }

  std::any Read(
      const PreprocessingDatasetMetadata &metadata,
      const std::string &datasetDirectory) const override;
};
