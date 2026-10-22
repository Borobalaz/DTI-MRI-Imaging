#include "Preprocessing/MriPreprocessingRunner.h"

#include <any>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "Preprocessing/MriTractographySettings.h"
#include "Preprocessing/io/PreprocessingDatasetFormats.h"
#include "Preprocessing/io/dti/DtiPreprocessingParameters.h"
#include "Preprocessing/preprocessors/MriToDtiPreprocessor.h"

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

  // Snapshot the tractography settings so the pipeline and the provenance below both see
  // exactly the same values, even if the caller keeps editing its settings object meanwhile.
  MriPreprocessingRequest preprocessingRequest = request.preprocessingRequest;
  preprocessingRequest.tractographySettings = request.preprocessingRequest.tractographySettings
      ? std::make_shared<MriTractographySettings>(*request.preprocessingRequest.tractographySettings)
      : std::make_shared<MriTractographySettings>();

  // Run preprocessing pipeline (MriPreprocessingCancelled propagates to the caller)
  MriToDtiPreprocessor preprocessor;
  MriPreprocessingRunnerResult runnerResult;
  runnerResult.preprocessingResult = preprocessor.Process(preprocessingRequest, options);

  // Save volume channels, meshes, and provenance metadata to disk
  const MriTractographySettings &tractographySettings = *preprocessingRequest.tractographySettings;
  const std::string parametersJson = BuildDtiPreprocessingParametersJson(
      preprocessingRequest, tractographySettings, tractographySettings.GetMatchedPresetName());

  const std::shared_ptr<IPreprocessingDatasetWriter> writer =
      GetDefaultPreprocessingDatasetFormatRegistry().FindWriter("dti");
  if (!writer)
  {
    throw std::runtime_error("No dataset writer registered for preprocessor type 'dti'.");
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
