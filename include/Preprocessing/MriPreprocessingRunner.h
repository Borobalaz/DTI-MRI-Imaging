#pragma once

#include <string>
#include <vector>

#include "Preprocessing/MriPreprocessingPipeline.h"

struct MriPreprocessingRunnerRequest
{
  MriPreprocessingRequest preprocessingRequest;
  std::string outputDirectory;
  std::string outputBasename = "dti_proxy";
};

struct MriPreprocessingRunnerResult
{
  bool success = false;
  std::string message;

  MriPreprocessingResult preprocessingResult;
  std::vector<std::string> writtenFiles;
};

class MriPreprocessingRunner
{
public:
  MriPreprocessingRunner();

  MriPreprocessingRunnerResult Run(const MriPreprocessingRunnerRequest& request) const;

  static std::string BuildSummary(const MriPreprocessingRunnerResult& result);
};