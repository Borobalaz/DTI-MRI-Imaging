#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "Threading/Cancellation.h"
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
  // Tractography seed positions in the same normalized object space as streamlineMesh's
  // vertices (see DtiFiberTractographyStage::VoxelToObjectSpace), for visualizing seed
  // placement independently of whether a streamline was successfully traced from it.
  std::vector<glm::vec3> seedPoints;
  MriPreprocessingReport report;
};

/**
 * @brief Thrown when a pipeline run stops because its cancellation token was signalled.
 *        Callers should treat it as a cancellation, not as a preprocessing failure.
 */
class MriPreprocessingCancelled : public OperationCancelled
{
public:
  MriPreprocessingCancelled() : OperationCancelled("MRI preprocessing cancelled") {}
};

struct MriPreprocessingProgress
{
  size_t stageIndex = 0; // 0-based index of the running stage
  size_t stageCount = 0;
  std::string stageName;
  float stageFraction = 0.0f; // completion of the running stage in [0,1]
};

struct MriPreprocessingExecutionOptions
{
  CancellationToken cancellation;
  // Invoked on the thread executing the pipeline (e.g. a background worker), not the caller's.
  std::function<void(const MriPreprocessingProgress&)> onProgress;
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

  // Stages may call these to stop early / report progress within a long-running stage.
  void ThrowIfCancelled() const;
  void ReportStageFraction(float fraction) const;

  MriPreprocessingRequest request;
  MriPreprocessingReport report;

  // execution control, set by MriPreprocessingPipeline::Execute
  CancellationToken cancellation;
  std::function<void(const MriPreprocessingProgress&)> onProgress;
  MriPreprocessingProgress currentStage;

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
  std::vector<glm::vec3> outputSeedPoints;
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
  // Checks for cancellation and reports progress between stages; throws
  // MriPreprocessingCancelled when options.cancellation is signalled.
  MriPreprocessingResult Execute(const MriPreprocessingRequest& request,
                                 const MriPreprocessingExecutionOptions& options = {}) const;

private:
  std::vector<std::unique_ptr<IMriPreprocessingStage>> stages;
};