#include "Preprocessing/MriPreprocessingPipeline.h"

#include <algorithm>

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
 * @brief Throw MriPreprocessingCancelled if this run's cancellation token was signalled.
 *
 */
void MriPreprocessingContext::ThrowIfCancelled() const
{
  if (cancellation.IsCancellationRequested())
  {
    throw MriPreprocessingCancelled();
  }
}

/**
 * @brief Report the completion fraction of the currently running stage to the progress callback.
 *
 * @param fraction completion of the running stage in [0,1]
 */
void MriPreprocessingContext::ReportStageFraction(float fraction) const
{
  if (!onProgress)
  {
    return;
  }

  MriPreprocessingProgress progress = currentStage;
  progress.stageFraction = std::clamp(fraction, 0.0f, 1.0f);
  onProgress(progress);
}

/**
 * @brief Execute the preprocessing pipeline with the given request, checking for cancellation
 *        and reporting progress between stages.
 *
 * @param request
 * @param options cancellation token and progress callback (the callback runs on this thread)
 * @return MriPreprocessingResult
 */
MriPreprocessingResult MriPreprocessingPipeline::Execute(const MriPreprocessingRequest& request,
                                                         const MriPreprocessingExecutionOptions& options) const
{
  MriPreprocessingContext context(request);
  context.cancellation = options.cancellation;
  context.onProgress = options.onProgress;
  context.currentStage.stageCount = stages.size();

  for (size_t stageIndex = 0; stageIndex < stages.size(); ++stageIndex)
  {
    const std::unique_ptr<IMriPreprocessingStage>& stage = stages[stageIndex];
    if (!stage)
    {
      continue;
    }

    context.ThrowIfCancelled();

    context.currentStage.stageIndex = stageIndex;
    context.currentStage.stageName = stage->Name();
    context.ReportStageFraction(0.0f);

    stage->Execute(context);
    context.report.executedStages.push_back(stage->Name());

    context.ReportStageFraction(1.0f);
  }

  return MriPreprocessingResult{
    std::move(context.outputDtiChannels),
    std::move(context.outputSurfaceMesh),
    std::move(context.outputStreamlineMesh),
    std::move(context.outputSeedPoints),
    std::move(context.outputShVolume),
    std::move(context.report)
  };
}
