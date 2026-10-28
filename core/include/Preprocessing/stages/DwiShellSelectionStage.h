#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

// Splits the DWI frames into b0 frames (b <= 50) and b-value shells (greedy clustering with
// MriFodfSettings::shellTolerance), selects the highest shell for CSD and warns about the others.
// Writes context.shellSelection.
class DwiShellSelectionStage final : public IMriPreprocessingStage
{
public:
  const char* Name() const override;
  void Execute(MriPreprocessingContext& context) const override;
};
