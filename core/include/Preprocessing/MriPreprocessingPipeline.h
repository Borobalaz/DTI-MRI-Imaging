#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "Volume/VolumeData.h"
#include "Volume/DTIVolume.h"

class MriTractographySettings;

struct MriPreprocessingRequest
{
  std::string dwiVolumePath;
  std::string bvalPath;
  std::string bvecPath;
  std::shared_ptr<MriTractographySettings> tractographySettings;
};

struct MriPreprocessingReport
{
  std::string sourceVolumePath;
  std::vector<std::string> executedStages;
  std::vector<std::string> warnings;
};

struct MriPreprocessingResult
{
  // Today's DTI (D-tensor) output; a future reconstruction model (e.g. fODF) would need
  // its own output shape alongside this one.
  DTIVolumeChannels dtiChannels;
  std::shared_ptr<Mesh> surfaceMesh;
  std::shared_ptr<Mesh> streamlineMesh;
  MriPreprocessingReport report;
};

/**
 * @brief DTO carried through the staged MRI preprocessing pipeline; carries the request,
 *        report, intermediate computed data, and the in-progress output as stages mutate
 *        it in order.
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
  DTIVolumeChannels outputDtiChannels;
  std::shared_ptr<Mesh> outputSurfaceMesh;
  std::shared_ptr<Mesh> outputStreamlineMesh;
};

/**
 * @brief A single stage of a generic staged MRI preprocessing pipeline. Each stage performs
 *        one task - either generic to any reconstruction model (e.g. input validation,
 *        gradient normalization) or specific to the reconstruction model currently wired
 *        into the pipeline (e.g. DTI tensor synthesis).
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
 * @brief An ordered sequence of IMriPreprocessingStages executed to turn raw MRI input into
 *        reconstructed output. This pipeline instance is currently wired for DTI output (see
 *        MriPreprocessingResult::dtiChannels); a future reconstruction model such as fODF
 *        would need its own result/context output shape alongside this one.
 */
class MriPreprocessingPipeline
{
public:
  MriPreprocessingPipeline& AddStage(std::unique_ptr<IMriPreprocessingStage> stage);
  MriPreprocessingResult Execute(const MriPreprocessingRequest& request) const;

private:
  std::vector<std::unique_ptr<IMriPreprocessingStage>> stages;
};