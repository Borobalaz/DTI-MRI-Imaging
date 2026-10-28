#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

// Estimates the single-fibre CSD response (Tournier 2007) from the top-N raw-FA voxels by a joint
// zonal SH fit of their eigenvector-aligned shell signals. Needs the raw tensor/EV/FA/L1 channels,
// so it must run before DWINormalizationStage. Writes context.outputFodfResponse.
class FodfResponseEstimationStage final : public IMriPreprocessingStage
{
public:
  const char* Name() const override;
  void Execute(MriPreprocessingContext& context) const override;
};
