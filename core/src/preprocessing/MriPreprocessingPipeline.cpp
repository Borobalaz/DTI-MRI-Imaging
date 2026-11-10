#include "Preprocessing/MriPreprocessingPipeline.h"

#include <algorithm>
#include <exception>
#include <utility>

#include "foundation/log/EngineLog.h"

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
    stages.push_back(StageEntry{std::move(stage), std::nullopt});
  }

  return *this;
}

/**
 * @brief Add a group of stages whose failure degrades the run instead of failing it.
 *
 * @param name group name used in the report warning (e.g. "fODF reconstruction")
 * @param groupStages stages run in order; null entries are ignored
 * @param onFailure resets the group's partial outputs; receives "<stage> failed: <what>"
 * @return MriPreprocessingPipeline&
 */
MriPreprocessingPipeline& MriPreprocessingPipeline::AddOptionalStageGroup(
  std::string name,
  std::vector<std::unique_ptr<IMriPreprocessingStage>> groupStages,
  StageGroupFailureHandler onFailure)
{
  const size_t groupIndex = optionalGroups.size();
  optionalGroups.push_back(OptionalGroup{std::move(name), std::move(onFailure)});

  for (std::unique_ptr<IMriPreprocessingStage>& stage : groupStages)
  {
    if (stage)
    {
      stages.push_back(StageEntry{std::move(stage), groupIndex});
    }
  }

  return *this;
}

/**
 * @brief Run one stage of an optional group, converting any non-cancellation failure into a
 *        report warning plus the group's failure handler.
 *
 * @param entry grouped stage to run
 * @param context pipeline context
 * @return true if the stage succeeded, false if it failed and the group must be skipped
 */
bool MriPreprocessingPipeline::ExecuteOptionalStage(const StageEntry& entry, MriPreprocessingContext& context) const
{
  try
  {
    entry.stage->Execute(context);
    return true;
  }
  catch (const OperationCancelled&)
  {
    throw;
  }
  catch (const std::exception& exception)
  {
    const OptionalGroup& group = optionalGroups[*entry.groupIndex];
    const std::string reason = std::string(entry.stage->Name()) + " failed: " + exception.what();
    const std::string warning = group.name + " unavailable: " + reason + ". Continuing without it.";
    context.report.warnings.push_back(warning);
    EngineLog::Warning(warning, "MriPreprocessing");
    if (group.onFailure)
    {
      group.onFailure(context, reason);
    }
    return false;
  }
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

  std::vector<bool> failedGroups(optionalGroups.size(), false);
  for (size_t stageIndex = 0; stageIndex < stages.size(); ++stageIndex)
  {
    const StageEntry& entry = stages[stageIndex];
    if (!entry.stage)
    {
      continue;
    }

    // Stages after a failed stage of the same group are skipped (and not reported as executed).
    if (entry.groupIndex && failedGroups[*entry.groupIndex])
    {
      continue;
    }

    context.ThrowIfCancelled();

    context.currentStage.stageIndex = stageIndex;
    context.currentStage.stageName = entry.stage->Name();
    context.ReportStageFraction(0.0f);

    if (entry.groupIndex)
    {
      if (!ExecuteOptionalStage(entry, context))
      {
        failedGroups[*entry.groupIndex] = true;
        continue;
      }
    }
    else
    {
      entry.stage->Execute(context);
    }
    context.report.executedStages.push_back(entry.stage->Name());

    context.ReportStageFraction(1.0f);
  }

  // context.dwiSeries is deliberately not copied, so the cached series is freed with the context.
  MriPreprocessingResult result;
  result.dtiChannels = std::move(context.outputDtiChannels);
  result.surfaceMesh = std::move(context.outputSurfaceMesh);
  result.streamlineMesh = std::move(context.outputStreamlineMesh);
  result.seedPoints = std::move(context.outputSeedPoints);
  result.shVolume = std::move(context.outputShVolume);
  result.fodfResponse = std::move(context.outputFodfResponse);
  result.fodfShVolume = std::move(context.outputFodfShVolume);
  result.fodfIgnoredShellBValues = std::move(context.shellSelection.ignoredShellBValues);
  result.fodfMaps = std::move(context.outputFodfMaps);
  result.fodfStreamlineMesh = std::move(context.outputFodfStreamlineMesh);
  result.fodfAbsoluteAmplitudeCutoff = context.fodfAbsoluteAmplitudeCutoff;
  result.fodfUnavailableReason = std::move(context.fodfUnavailableReason);
  result.report = std::move(context.report);
  return result;
}
