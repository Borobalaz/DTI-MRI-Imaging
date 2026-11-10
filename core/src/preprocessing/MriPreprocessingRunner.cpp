#include "Preprocessing/MriPreprocessingRunner.h"

#include <any>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "Preprocessing/MriFodfSettings.h"
#include "Preprocessing/MriFodfTractographySettings.h"
#include "Preprocessing/MriTractographySettings.h"
#include "Preprocessing/io/PreprocessingDatasetFormats.h"
#include "Preprocessing/io/dmri/DmriDatasetLayout.h"
#include "Preprocessing/io/dmri/DmriPreprocessingParameters.h"
#include "Preprocessing/preprocessors/MriToDmriPreprocessor.h"

namespace
{
  // A copy of the caller's settings, or defaults when it passed none.
  template <typename Settings>
  std::shared_ptr<Settings> SnapshotSettings(const std::shared_ptr<Settings> &settings)
  {
    return settings ? std::make_shared<Settings>(*settings) : std::make_shared<Settings>();
  }
}

MriPreprocessingRunner::MriPreprocessingRunner() = default;

/**
 * @brief Run the MRI preprocessing pipeline with the given request and persist its output
 *        (volume channels, meshes, and provenance metadata) to disk.
 *
 * @param request
 * @param options cancellation/progress hooks forwarded to the pipeline; a cancelled run throws
 *        MriPreprocessingCancelled instead of returning a result
 * @return MriPreprocessingRunnerResult
 */
MriPreprocessingRunnerResult MriPreprocessingRunner::Run(const MriPreprocessingRunnerRequest &request,
                                                         const MriPreprocessingExecutionOptions &options) const
{
  // Validate request
  if (request.preprocessingRequest.dwiVolumePath.empty() ||
      request.preprocessingRequest.bvalPath.empty() ||
      request.preprocessingRequest.bvecPath.empty())
  {
    throw std::invalid_argument(
      "Runner requires explicit preprocessingRequest dwiVolumePathOverride, bvalPathOverride, and bvecPathOverride.");
  }

  if (request.outputDirectory.empty())
  {
    throw std::invalid_argument("Runner requires outputDirectory.");
  }

  // Create output directory if it doesn't exist
  const std::filesystem::path outputDirectoryPath(request.outputDirectory);
  std::filesystem::create_directories(outputDirectoryPath);

  const std::string outputBaseName =
      request.outputBasename.empty() ? std::string("mri_proxy") : request.outputBasename;

  // Snapshot all three settings objects so the pipeline and the provenance below both see
  // exactly the same values, even if the caller keeps editing its settings objects meanwhile.
  MriPreprocessingRequest preprocessingRequest = request.preprocessingRequest;
  preprocessingRequest.tractographySettings = SnapshotSettings(request.preprocessingRequest.tractographySettings);
  preprocessingRequest.fodfSettings = SnapshotSettings(request.preprocessingRequest.fodfSettings);
  preprocessingRequest.fodfTractographySettings =
      SnapshotSettings(request.preprocessingRequest.fodfTractographySettings);

  // Run preprocessing pipeline (MriPreprocessingCancelled propagates to the caller)
  MriToDmriPreprocessor preprocessor;
  MriPreprocessingRunnerResult runnerResult;
  runnerResult.preprocessingResult = preprocessor.Process(preprocessingRequest, options);

  // Save volume channels, fODF outputs, meshes, and provenance metadata to disk
  const std::string parametersJson = BuildDmriPreprocessingParametersJson(
      preprocessingRequest,
      *preprocessingRequest.tractographySettings,
      *preprocessingRequest.fodfSettings,
      *preprocessingRequest.fodfTractographySettings,
      runnerResult.preprocessingResult);

  const std::string preprocessorType = DmriDatasetLayout::kPreprocessorType;
  const std::shared_ptr<IPreprocessingDatasetWriter> writer =
      GetDefaultPreprocessingDatasetFormatRegistry().FindWriter(preprocessorType);
  if (!writer)
  {
    throw std::runtime_error("No dataset writer registered for preprocessor type '" + preprocessorType + "'.");
  }

  const PreprocessingDatasetMetadata metadata = writer->Write(
      std::any(runnerResult.preprocessingResult), outputDirectoryPath.string(), outputBaseName, parametersJson);

  const std::filesystem::path datasetDirectory = outputDirectoryPath / outputBaseName;
  runnerResult.writtenFiles.push_back((datasetDirectory / "metadata.json").string());
  for (const PreprocessingDatasetFileEntry &entry : metadata.files)
  {
    runnerResult.writtenFiles.push_back((datasetDirectory / entry.relativePath).string());
  }

  // Mark as successful if we reached this point without exceptions
  runnerResult.success = true;
  runnerResult.message = "Preprocessing completed and channels were written to " + datasetDirectory.string();
  return runnerResult;
}

/**
 * @brief Create a human-readable summary string of the preprocessing result, including success status, messages, executed stages, warnings, and written files.
 *
 * @param result
 * @return std::string
 */
std::string MriPreprocessingRunner::BuildSummary(const MriPreprocessingRunnerResult &result)
{
  std::ostringstream summary;
  summary << (result.success ? "SUCCESS" : "FAILED") << "\n";

  if (!result.message.empty())
  {
    summary << "Message: " << result.message << "\n";
  }

  if (!result.preprocessingResult.report.sourceVolumePath.empty())
  {
    summary << "Source volume: " << result.preprocessingResult.report.sourceVolumePath << "\n";
  }

  if (!result.preprocessingResult.report.executedStages.empty())
  {
    summary << "Executed stages:" << "\n";
    for (const std::string &stage : result.preprocessingResult.report.executedStages)
    {
      summary << "  - " << stage << "\n";
    }
  }

  if (!result.writtenFiles.empty())
  {
    summary << "Written files:" << "\n";
    for (const std::string &filePath : result.writtenFiles)
    {
      summary << "  - " << filePath << "\n";
    }
  }

  if (!result.preprocessingResult.report.warnings.empty())
  {
    summary << "Warnings:" << "\n";
    for (const std::string &warning : result.preprocessingResult.report.warnings)
    {
      summary << "  - " << warning << "\n";
    }
  }

  return summary.str();
}
