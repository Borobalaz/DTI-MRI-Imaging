#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "VolumeData.h"
#include "DTIVolume.h"

struct MriPreprocessingRequest
{
  std::string dwiVolumePath;
  std::string bvalPath;
  std::string bvecPath;
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

/**
 * @brief DTO object of the processor pipeline. 
 *        It carries the request, report, result and intermediate data. 
 * 
 */
struct MriPreprocessingContext
{
  explicit MriPreprocessingContext(MriPreprocessingRequest request) : request(std::move(request)) {}

  MriPreprocessingRequest request;
  MriPreprocessingReport report;

  // inputs
  std::string selectedDwiVolumePath;
  std::string selectedBvalPath;
  std::string selectedBvecPath;
  
  // calculated
  std::vector<float> bValues;
  std::vector<glm::vec3> gradientDirections;
  bool gradientMetadataValid = false;

  // output
  DTIVolumeChannels outputChannels;
};

/**
 * @brief A stage of the preprocessor. Each stage performs a specific task in the preprocessing pipeline, 
 *        such as normalizing bvectors or synthesizing the D tensor.
 * 
 */
class IMriPreprocessingStage
{
public:
  virtual ~IMriPreprocessingStage() = default;
  virtual const char* Name() const = 0;
  virtual void Execute(MriPreprocessingContext& context) const = 0;
};

/**
 * @brief A vector of stages that can be executed in order.
 *        Returns a Result object containing the output DTIVolumeChannels (D tensor)
 */
class MriPreprocessingPipeline
{
public:
  MriPreprocessingPipeline& AddStage(std::unique_ptr<IMriPreprocessingStage> stage);
  MriPreprocessingResult Execute(const MriPreprocessingRequest& request) const;

private:
  std::vector<std::unique_ptr<IMriPreprocessingStage>> stages;
};