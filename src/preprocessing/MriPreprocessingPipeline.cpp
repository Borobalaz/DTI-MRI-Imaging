#include "Preprocessing/MriPreprocessingPipeline.h"

/**
 * @brief Add a preprocessing stage to the pipeline.
 * 
 * @param stage 
 * @return MriPreprocessingPipeline& 
 */
MriPreprocessingPipeline& MriPreprocessingPipeline::AddStage(std::unique_ptr<IMriPreprocessingStage> stage)
{
  if (stage)
  {
    stages.push_back(std::move(stage));
  }

  return *this;
}

/**
 * @brief Execute the preprocessing pipeline with the given request.
 * 
 * @param request 
 * @return MriPreprocessingResult 
 */
MriPreprocessingResult MriPreprocessingPipeline::Execute(const MriPreprocessingRequest& request) const
{
  MriPreprocessingContext context(request);

  for (const std::unique_ptr<IMriPreprocessingStage>& stage : stages)
  {
    if (!stage)
    {
      continue;
    }

    stage->Execute(context);
    context.report.executedStages.push_back(stage->Name());
  }

  return MriPreprocessingResult{
    std::move(context.outputChannels),
    std::move(context.report)
  };
}
