#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

// Constrained spherical deconvolution (Tournier 2007) of the selected shell with the estimated
// response: per-voxel iterative Tikhonov non-negativity, slice-parallel. Writes
// context.outputFodfShVolume (lmax-8 layout, zero-padded above the CSD lmax, masked by raw FA).
class FodfCsdStage final : public IMriPreprocessingStage
{
public:
  const char* Name() const override;
  void Execute(MriPreprocessingContext& context) const override;
};
