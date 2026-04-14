#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

class DWIFiberTractographyStage final : public IMriPreprocessingStage
{
public:
  const char *Name() const override;
  void Execute(MriPreprocessingContext &context) const override;
};

std::unique_ptr<IMriPreprocessingStage> CreateDwiFiberTractographyStage();
