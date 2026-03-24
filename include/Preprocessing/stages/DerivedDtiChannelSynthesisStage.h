#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

class DerivedDtiChannelSynthesisStage final : public IMriPreprocessingStage
{
public:
  const char* Name() const override;
  void Execute(MriPreprocessingContext& context) const override;
};
