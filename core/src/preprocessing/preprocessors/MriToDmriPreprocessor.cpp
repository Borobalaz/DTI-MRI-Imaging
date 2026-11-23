#include "Preprocessing/preprocessors/MriToDmriPreprocessor.h"

#include "Preprocessing/stages/CommonPreprocessingStages.h"
#include "Preprocessing/stages/dti/DtiPreprocessingStages.h"
#include "Preprocessing/stages/fodf/FodfPreprocessingStages.h"

MriToDmriPreprocessor::MriToDmriPreprocessor()
{
  pipeline
    .AddStage(CreateDwiInputValidationStage())      // also caches the DWI series
    .AddStage(CreateDwiGradientNormalizationStage())
    .AddStage(CreateDtiTensorSynthesisStage())      // reads the cached series
    .AddStage(CreateDtiPrincipalEigenvectorStage())
    .AddStage(CreateDtiScalarSynthesisStage())
    // Everything from here up to (and including) the fODF stages needs the raw channels, so it must
    // precede DwiNormalizationStage.
    .AddStage(CreateDtiTensorShStage())
    .AddStage(CreateDwiBrainSurfaceMeshStage())
    .AddStage(CreateDtiFiberTractographyStage())    // also stores the voxel-space seeds
    // Shell selection throws without b0 frames; only the fODF stages read it.
    .AddStage(CreateDwiShellSelectionStage())
    .AddStage(CreateFodfResponseEstimationStage())
    .AddStage(CreateFodfCsdStage())
    .AddStage(CreateFodfPeakExtractionStage())
    .AddStage(CreateFodfFiberTractographyStage())   // reuses the DTI tracker's seeds
    // Normalizes the 16 DTI channels only; the fODF maps stay raw.
    .AddStage(CreateDwiNormalizationStage());
}

MriPreprocessingResult MriToDmriPreprocessor::Process(const MriPreprocessingRequest& request,
                                                      const MriPreprocessingExecutionOptions& options) const
{
  return pipeline.Execute(request, options);
}
