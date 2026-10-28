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
    // Needs the raw tensor, so it must precede DwiNormalizationStage.
    .AddStage(CreateDtiTensorShStage())
    .AddStage(CreateDwiBrainSurfaceMeshStage())
    .AddStage(CreateDtiFiberTractographyStage())
    .AddStage(CreateDwiNormalizationStage());
}

MriPreprocessingResult MriToDtiPreprocessor::Process(const MriPreprocessingRequest& request,
                                                     const MriPreprocessingExecutionOptions& options) const
{
  return pipeline.Execute(request, options);
}
