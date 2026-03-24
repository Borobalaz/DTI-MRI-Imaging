#include "Preprocessing/MriToDtiPreprocessor.h"

#include <memory>

#include "Preprocessing/MriPreprocessingStages.h"

MriToDtiPreprocessor::MriToDtiPreprocessor()
{
  pipeline
    .AddStage(CreateDatasetDiscoveryStage())
    .AddStage(CreateScalarVolumeLoadStage())
    .AddStage(CreateIntensityNormalizationStage())
    .AddStage(CreateDerivedDtiChannelSynthesisStage());
}

MriPreprocessingResult MriToDtiPreprocessor::Process(const MriPreprocessingRequest& request) const
{
  return pipeline.Execute(request);
}
