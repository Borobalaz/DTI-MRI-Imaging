#include "Preprocessing/MriToDtiPreprocessor.h"

#include <memory>

#include "Preprocessing/MriPreprocessingStages.h"

MriToDtiPreprocessor::MriToDtiPreprocessor()
{
  pipeline
    .AddStage(CreateDwiInputValidationStage())
    .AddStage(CreateDwiGradientNormalizationStage())
    .AddStage(CreateDwiTensorSynthesisStage())
    .AddStage(CreateDwiPrincipalEigenvectorStage())
    .AddStage(CreateDwiScalarSynthesisStage())
    .AddStage(CreateDwiBrainSurfaceMeshStage())
    .AddStage(CreateDwiFiberTractographyStage())
    .AddStage(CreateDwiNormalizationStage());
}

MriPreprocessingResult MriToDtiPreprocessor::Process(const MriPreprocessingRequest& request) const
{
  return pipeline.Execute(request);
}
