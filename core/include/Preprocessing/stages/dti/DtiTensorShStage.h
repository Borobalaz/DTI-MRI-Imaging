#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

// Converts each voxel's raw diffusion tensor into spherical-harmonic coefficients (RealSH, lmax 8)
// of its tensor ODF, for the SH glyph field. Must run before DWINormalizationStage, which rescales
// the tensor channels and so destroys the tensor.
class DtiTensorShStage final : public IMriPreprocessingStage
{
public:
  const char* Name() const override;
  void Execute(MriPreprocessingContext& context) const override;
};
