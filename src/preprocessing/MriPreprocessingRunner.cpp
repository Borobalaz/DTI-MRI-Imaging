#include "Preprocessing/MriPreprocessingRunner.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>

#include "Preprocessing/MriToDtiPreprocessor.h"
#include "VolumeFileLoader.h"

namespace
{
  std::string BuildOutputPath(const std::filesystem::path& outputDirectory,
                              const std::string& outputBasename,
                              const std::string& channelSuffix)
  {
    return (outputDirectory / (outputBasename + "_" + channelSuffix + ".vxa")).string();
  }

  void SaveOrThrow(const std::string& outputPath, const VolumeData<float>& volume)
  {
    if (!VolumeFileLoader::Save(outputPath, volume))
    {
      throw std::runtime_error("Failed to save preprocessed channel: " + outputPath);
    }
  }
}

MriPreprocessingRunner::MriPreprocessingRunner() = default;

MriPreprocessingRunnerResult MriPreprocessingRunner::Run(const MriPreprocessingRunnerRequest& request) const
{
  if (request.preprocessingRequest.datasetRootPath.empty())
  {
    throw std::invalid_argument("Runner requires preprocessingRequest.datasetRootPath.");
  }

  if (request.outputDirectory.empty())
  {
    throw std::invalid_argument("Runner requires outputDirectory.");
  }

  const std::filesystem::path outputDirectoryPath(request.outputDirectory);
  std::filesystem::create_directories(outputDirectoryPath);

  const std::string outputBaseName =
    request.outputBasename.empty() ? std::string("dti_proxy") : request.outputBasename;

  MriToDtiPreprocessor preprocessor;
  MriPreprocessingRunnerResult runnerResult;
  runnerResult.preprocessingResult = preprocessor.Process(request.preprocessingRequest);

  const DTIVolumeChannels& channels = runnerResult.preprocessingResult.channels;

  const std::string faPath = BuildOutputPath(outputDirectoryPath, outputBaseName, "fa");
  SaveOrThrow(faPath, channels.fa);
  runnerResult.writtenFiles.push_back(faPath);

  if (channels.md.has_value())
  {
    const std::string mdPath = BuildOutputPath(outputDirectoryPath, outputBaseName, "md");
    SaveOrThrow(mdPath, *channels.md);
    runnerResult.writtenFiles.push_back(mdPath);
  }

  if (channels.ad.has_value())
  {
    const std::string adPath = BuildOutputPath(outputDirectoryPath, outputBaseName, "ad");
    SaveOrThrow(adPath, *channels.ad);
    runnerResult.writtenFiles.push_back(adPath);
  }

  if (channels.rd.has_value())
  {
    const std::string rdPath = BuildOutputPath(outputDirectoryPath, outputBaseName, "rd");
    SaveOrThrow(rdPath, *channels.rd);
    runnerResult.writtenFiles.push_back(rdPath);
  }

  runnerResult.success = true;
  runnerResult.message = "Preprocessing completed and channels were written to " + outputDirectoryPath.string();
  return runnerResult;
}

std::string MriPreprocessingRunner::BuildSummary(const MriPreprocessingRunnerResult& result)
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
    for (const std::string& stage : result.preprocessingResult.report.executedStages)
    {
      summary << "  - " << stage << "\n";
    }
  }

  if (!result.writtenFiles.empty())
  {
    summary << "Written files:" << "\n";
    for (const std::string& filePath : result.writtenFiles)
    {
      summary << "  - " << filePath << "\n";
    }
  }

  if (!result.preprocessingResult.report.warnings.empty())
  {
    summary << "Warnings:" << "\n";
    for (const std::string& warning : result.preprocessingResult.report.warnings)
    {
      summary << "  - " << warning << "\n";
    }
  }

  return summary.str();
}