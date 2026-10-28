#include "Preprocessing/preprocessors/MriToFodfPreprocessor.h"

#include <memory>

#include "Preprocessing/stages/CommonPreprocessingStages.h"
#include "Preprocessing/stages/dti/DtiPreprocessingStages.h"
#include "Preprocessing/stages/fodf/FodfPreprocessingStages.h"

MriToFodfPreprocessor::MriToFodfPreprocessor()
{
  pipeline
    .AddStage(CreateDwiInputValidationStage())      // also caches the DWI series
    .AddStage(CreateDwiGradientNormalizationStage())
    .AddStage(CreateDwiShellSelectionStage())
    .AddStage(CreateDtiTensorSynthesisStage())      // reads the cached series
    .AddStage(CreateDtiPrincipalEigenvectorStage())
    .AddStage(CreateDtiScalarSynthesisStage())
    // The next three need raw (unnormalized) channels, so they must precede DwiNormalizationStage.
    .AddStage(CreateDtiTensorShStage())
    .AddStage(CreateFodfResponseEstimationStage())
    .AddStage(CreateFodfCsdStage())
    .AddStage(CreateDwiNormalizationStage());
}

MriPreprocessingResult MriToFodfPreprocessor::Process(const MriPreprocessingRequest& request,
                                                      const MriPreprocessingExecutionOptions& options) const
{
  return pipeline.Execute(request, options);
}
