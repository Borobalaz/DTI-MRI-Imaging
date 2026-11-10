#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

// Extracts up to 3 fODF peaks per CSD-solved voxel (c00 > 0) from context.outputFodfShVolume,
// derives the absolute tracking cutoff (relative cutoff x the 95th percentile of peak-1 amplitude)
// and keeps only peaks at or above it. Writes the raw maps (AFD, NuFO, peak directions and
// amplitudes) to context.outputFodfMaps and the cutoff to context.fodfAbsoluteAmplitudeCutoff.
// Throws when the SH volume is missing/invalid or no voxel has a peak.
class FodfPeakExtractionStage final : public IMriPreprocessingStage
{
public:
  const char* Name() const override;
  void Execute(MriPreprocessingContext& context) const override;
};
