#include "Preprocessing/MriToDtiPreprocessor.h"

#include <memory>

#include "Preprocessing/MriPreprocessingStages.h"

MriToDtiPreprocessor::MriToDtiPreprocessor()
{
  pipeline
    .AddStage(CreateDwiGradientNormalizationStage())
    .AddStage(CreateDwiTensorSynthesisStage());
    //.AddStage(CreateDwiNormalizationStage());
}

MriPreprocessingResult MriToDtiPreprocessor::Process(const MriPreprocessingRequest& request) const
{
  return pipeline.Execute(request);
}
