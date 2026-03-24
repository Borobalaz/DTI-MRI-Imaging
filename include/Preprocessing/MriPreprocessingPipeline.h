#pragma once

#include <memory>
#include <string>
#include <vector>

#include "VolumeData.h"
#include "DTIVolume.h"

struct MriPreprocessingRequest
{
  std::string datasetRootPath;
  std::string preferredSubjectId;
  std::string preferredSessionId;
  bool preferAnatomicalVolumes = true;
  bool generateAllChannels = true;
};

struct MriPreprocessingReport
{
  std::string sourceVolumePath;
  std::vector<std::string> executedStages;
  std::vector<std::string> warnings;
};

struct MriPreprocessingResult
{
  DTIVolumeChannels channels;
  MriPreprocessingReport report;
};

struct MriPreprocessingContext
{
  explicit MriPreprocessingContext(MriPreprocessingRequest request)
    : request(std::move(request))
  {
  }

  MriPreprocessingRequest request;
  MriPreprocessingReport report;

  std::vector<std::string> candidateVolumePaths;
  std::string selectedSourceVolumePath;

  VolumeData<float> loadedVolume;
  VolumeData<float> normalizedVolume;

  DTIVolumeChannels outputChannels;
};

class IMriPreprocessingStage
{
public:
  virtual ~IMriPreprocessingStage() = default;
  virtual const char* Name() const = 0;
  virtual void Execute(MriPreprocessingContext& context) const = 0;
};

class MriPreprocessingPipeline
{
public:
  MriPreprocessingPipeline& AddStage(std::unique_ptr<IMriPreprocessingStage> stage);
  MriPreprocessingResult Execute(const MriPreprocessingRequest& request) const;

private:
  std::vector<std::unique_ptr<IMriPreprocessingStage>> stages;
};