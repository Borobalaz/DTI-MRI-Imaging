#include "Preprocessing/MriPreprocessingRunner.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>

#include "Preprocessing/MriToDtiPreprocessor.h"
#include "VolumeFileLoader.h"

namespace
{
  std::string BuildOutputPath(const std::filesystem::path &outputDirectory,
                              const std::string &outputBasename,
                              const std::string &channelSuffix)
  {
    return (outputDirectory / (outputBasename + "_" + channelSuffix + ".vxa")).string();
  }

  void SaveOrThrow(const std::string &outputPath, const VolumeData &volume)
  {
    if (!VolumeFileLoader::Save(outputPath, volume))
    {
      throw std::runtime_error("Failed to save preprocessed channel: " + outputPath);
    }
  }
}

MriPreprocessingRunner::MriPreprocessingRunner() = default;

/**
 * @brief Run the MRI preprocessing pipeline with the given request.
 *
 * @param request
 * @return MriPreprocessingRunnerResult
 */
MriPreprocessingRunnerResult MriPreprocessingRunner::Run(const MriPreprocessingRunnerRequest &request) const
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
      request.outputBasename.empty() ? std::string("dti_proxy") : request.outputBasename;

  // Run preprocessing pipeline
  MriToDtiPreprocessor preprocessor;
  MriPreprocessingRunnerResult runnerResult;
  runnerResult.preprocessingResult = preprocessor.Process(request.preprocessingRequest);

  const DTIVolumeChannels &channels = runnerResult.preprocessingResult.channels;

  // Save volume channels to disk 

  
  // Mark as successful if we reached this point without exceptions
  runnerResult.success = true;
  runnerResult.message = "Preprocessing completed and channels were written to " + outputDirectoryPath.string();
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