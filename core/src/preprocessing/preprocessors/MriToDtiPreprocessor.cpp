#include "Preprocessing/preprocessors/MriToDtiPreprocessor.h"

#include <memory>

#include "Preprocessing/stages/CommonPreprocessingStages.h"
#include "Preprocessing/stages/dti/DtiPreprocessingStages.h"

MriToDtiPreprocessor::MriToDtiPreprocessor()
{
  pipeline
    .AddStage(CreateDwiInputValidationStage())
    .AddStage(CreateDwiGradientNormalizationStage())
    .AddStage(CreateDtiTensorSynthesisStage())
    .AddStage(CreateDtiPrincipalEigenvectorStage())
    .AddStage(CreateDtiScalarSynthesisStage())
    .AddStage(CreateDwiBrainSurfaceMeshStage())
    .AddStage(CreateDtiFiberTractographyStage())
    .AddStage(CreateDwiNormalizationStage());
}

MriPreprocessingResult MriToDtiPreprocessor::Process(const MriPreprocessingRequest& request) const
{
  return pipeline.Execute(request);
}
