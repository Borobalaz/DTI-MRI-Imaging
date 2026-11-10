#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

// Deterministic, bidirectional fODF peak-following tractography from the DTI tracker's seeds
// (context.tractographySeedsVoxelSpace). Each step trilinearly interpolates the fODF SH and follows
// the peak closest to the previous direction (within the max angle, above
// context.fodfAbsoluteAmplitudeCutoff); it stops on amplitude, angle, bounds or the CSD mask.
// Writes context.outputFodfStreamlineMesh. Throws when the SH volume is invalid or the cutoff is
// not positive; no seeds or no surviving streamline only warn.
class FodfFiberTractographyStage final : public IMriPreprocessingStage
{
public:
  const char* Name() const override;
  void Execute(MriPreprocessingContext& context) const override;
};
