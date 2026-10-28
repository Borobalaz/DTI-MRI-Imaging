#include "Preprocessing/stages/DwiShellSelectionStage.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Preprocessing/MriFodfSettings.h"
#include "Preprocessing/stages/CommonPreprocessingStages.h"
#include "foundation/log/EngineLog.h"
#include "scene/volume/VolumeSeriesData.h"

namespace
{
  constexpr float kB0Threshold = 50.0f;

  struct Shell
  {
    double sum = 0.0;
    size_t count = 0;
    std::vector<size_t> frames;

    double Mean() const { return sum / static_cast<double>(count); }
  };

  std::string FormatInteger(double value)
  {
    return std::to_string(std::lround(value));
  }
}

/**
 * @brief Get the name of the preprocessing stage.
 *
 * @return const char*
 */
const char* DwiShellSelectionStage::Name() const
{
  return "DWI shell selection";
}

/**
 * @brief Cluster the diffusion-weighted b-values into shells, select the highest shell for CSD
 *        and record the b0 frames used for S0.
 *
 * @param context
 */
void DwiShellSelectionStage::Execute(MriPreprocessingContext& context) const
{
  const float shellTolerance = context.request.fodfSettings
                                   ? context.request.fodfSettings->GetShellTolerance()
                                   : MriFodfSettings{}.GetShellTolerance();

  if (!context.gradientMetadataValid)
  {
    throw std::runtime_error(
      "DWI shell selection: gradient metadata is missing; DWI gradient normalization must run first.");
  }
  if (!context.dwiSeries)
  {
    throw std::runtime_error(
      "DWI shell selection: DWI series is not cached; DWI input validation must run first.");
  }

  const std::vector<float>& bValues = context.bValues;
  const int frameCount = context.dwiSeries->GetFrameCount();
  if (frameCount < 0 || static_cast<size_t>(frameCount) != bValues.size())
  {
    throw std::runtime_error(
      "DWI shell selection: DWI frame count (" + std::to_string(frameCount) +
      ") does not match bval count (" + std::to_string(bValues.size()) + ").");
  }

  std::vector<size_t> b0Frames;
  std::vector<std::pair<float, size_t>> weighted;
  for (size_t i = 0; i < bValues.size(); ++i)
  {
    if (bValues[i] <= kB0Threshold)
    {
      b0Frames.push_back(i);
    }
    else
    {
      weighted.emplace_back(bValues[i], i);
    }
  }
  std::stable_sort(weighted.begin(), weighted.end(),
                   [](const std::pair<float, size_t>& lhs, const std::pair<float, size_t>& rhs)
                   { return lhs.first < rhs.first; });

  if (b0Frames.empty())
  {
    throw std::runtime_error(
      "DWI shell selection: no b0 volumes (b <= 50) found; CSD needs b0 frames for S0.");
  }
  if (weighted.empty())
  {
    throw std::runtime_error("DWI shell selection: no diffusion-weighted volumes (b > 50) found.");
  }

  // Greedy clustering in ascending b order: a new shell opens when b exceeds the running mean of
  // the current shell by more than the tolerance.
  std::vector<Shell> shells;
  for (const auto& [b, frame] : weighted)
  {
    if (shells.empty() || static_cast<double>(b) > shells.back().Mean() + static_cast<double>(shellTolerance))
    {
      shells.emplace_back();
    }
    Shell& shell = shells.back();
    shell.sum += static_cast<double>(b);
    ++shell.count;
    shell.frames.push_back(frame);
  }

  // Sorted input, so the last shell has the highest mean.
  Shell& selected = shells.back();
  std::sort(selected.frames.begin(), selected.frames.end());

  DwiShellSelection selection;
  selection.b0Frames = std::move(b0Frames);
  selection.shellFrames = selected.frames;
  selection.shellBValue = static_cast<float>(selected.Mean());
  for (size_t s = 0; s + 1 < shells.size(); ++s)
  {
    selection.ignoredShellBValues.push_back(static_cast<float>(shells[s].Mean()));
    context.report.warnings.push_back(
      "DWI shell selection: ignoring shell b=" + FormatInteger(shells[s].Mean()) + " s/mm^2 (" +
      std::to_string(shells[s].count) + " volumes); CSD uses only the highest shell b=" +
      FormatInteger(selected.Mean()) + " s/mm^2.");
  }

  EngineLog::Message(
    "Shell selection: " + std::to_string(selection.b0Frames.size()) + " b0 volumes, CSD shell b=" +
    FormatInteger(selected.Mean()) + " s/mm^2 with " + std::to_string(selection.shellFrames.size()) +
    " directions, " + std::to_string(selection.ignoredShellBValues.size()) + " other shell(s) ignored",
    "DwiShellSelection");

  context.shellSelection = std::move(selection);
}

/**
 * @brief Create a Dwi Shell Selection Stage object
 *
 * @return std::unique_ptr<IMriPreprocessingStage>
 */
std::unique_ptr<IMriPreprocessingStage> CreateDwiShellSelectionStage()
{
  return std::make_unique<DwiShellSelectionStage>();
}
