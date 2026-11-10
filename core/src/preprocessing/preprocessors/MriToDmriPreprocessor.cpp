#include "Preprocessing/preprocessors/MriToDmriPreprocessor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Preprocessing/stages/CommonPreprocessingStages.h"
#include "Preprocessing/stages/dti/DtiPreprocessingStages.h"
#include "Preprocessing/stages/fodf/FodfPreprocessingStages.h"

namespace
{
  constexpr const char* kFodfGroupName = "fODF reconstruction";

  // Drops whatever the fODF group produced before it failed, so the result is a clean DTI-only one.
  void ClearFodfOutputs(MriPreprocessingContext& context, const std::string& reason)
  {
    context.shellSelection = DwiShellSelection{};
    context.outputFodfResponse = FodfResponseFunction{};
    context.outputFodfShVolume = nullptr;
    context.outputFodfMaps = nullptr;
    context.outputFodfStreamlineMesh = nullptr;
    context.fodfAbsoluteAmplitudeCutoff = 0.0f;
    context.fodfUnavailableReason = reason;
  }

  std::vector<std::unique_ptr<IMriPreprocessingStage>> CreateFodfReconstructionStages()
  {
    std::vector<std::unique_ptr<IMriPreprocessingStage>> stages;
    // Shell selection throws without b0 frames and only the fODF stages read it, so it belongs here.
    stages.push_back(CreateDwiShellSelectionStage());
    stages.push_back(CreateFodfResponseEstimationStage());
    stages.push_back(CreateFodfCsdStage());
    stages.push_back(CreateFodfPeakExtractionStage());
    stages.push_back(CreateFodfFiberTractographyStage());   // reuses the DTI tracker's seeds
    return stages;
  }
}

MriToDmriPreprocessor::MriToDmriPreprocessor()
{
  pipeline
    .AddStage(CreateDwiInputValidationStage())      // also caches the DWI series
    .AddStage(CreateDwiGradientNormalizationStage())
    .AddStage(CreateDtiTensorSynthesisStage())      // reads the cached series
    .AddStage(CreateDtiPrincipalEigenvectorStage())
    .AddStage(CreateDtiScalarSynthesisStage())
    // Everything from here up to (and including) the fODF group needs the raw channels, so it must
    // precede DwiNormalizationStage.
    .AddStage(CreateDtiTensorShStage())
    .AddStage(CreateDwiBrainSurfaceMeshStage())
    .AddStage(CreateDtiFiberTractographyStage())    // also stores the voxel-space seeds
    .AddOptionalStageGroup(kFodfGroupName, CreateFodfReconstructionStages(), ClearFodfOutputs)
    // Normalizes the 16 DTI channels only; the fODF maps stay raw.
    .AddStage(CreateDwiNormalizationStage());
}

MriPreprocessingResult MriToDmriPreprocessor::Process(const MriPreprocessingRequest& request,
                                                      const MriPreprocessingExecutionOptions& options) const
{
  return pipeline.Execute(request, options);
}
