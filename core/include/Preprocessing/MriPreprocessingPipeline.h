#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Preprocessing/FodfResponseFunction.h"
#include "scene/glyphs/SHVolume.h"
#include "scene/object/Mesh.h"
#include "foundation/threading/Cancellation.h"
#include "scene/volume/VolumeData.h"
#include "scene/volume/DTIVolume.h"

class MriTractographySettings;
class MriFodfSettings;
class VolumeSeriesData;

struct MriPreprocessingRequest
{
  std::string dwiVolumePath;
  std::string bvalPath;
  std::string bvecPath;
  std::shared_ptr<MriTractographySettings> tractographySettings;
  std::shared_ptr<MriFodfSettings> fodfSettings; // fODF runs only; stages treat null as MriFodfSettings{} defaults
};

struct MriPreprocessingReport
{
  std::string sourceVolumePath;
  std::vector<std::string> executedStages;
  std::vector<std::string> warnings;
};

struct MriPreprocessingResult
{
  // Filled by MriToDtiPreprocessor (dtiChannels, meshes, seeds, tensor-ODF shVolume) or by MriToFodfPreprocessor (dtiChannels, shVolume, fodfResponse, fodfShVolume, fodfIgnoredShellBValues; no meshes/seeds).
  DTIVolumeChannels dtiChannels;
  std::shared_ptr<Mesh> surfaceMesh;
  std::shared_ptr<Mesh> streamlineMesh;
  // Tractography seed positions in the same normalized object space as streamlineMesh's
  // vertices (see DtiFiberTractographyStage::VoxelToObjectSpace), for visualizing seed
  // placement independently of whether a streamline was successfully traced from it.
  std::vector<glm::vec3> seedPoints;
  // DTI-derived SH (tensor ODF). Live runs; persisted only by the "fodf" dataset format.
  std::shared_ptr<const SHVolume> shVolume;
  // fODF (CSD) outputs; empty/null for DTI runs. fodfShVolume is always stored at lmax 8
  // (45 coefficients, zero-padded above the requested lmax), masked by the raw FA.
  FodfResponseFunction fodfResponse;
  std::shared_ptr<const SHVolume> fodfShVolume;
  // b-values of the DWI shells CSD ignored (only the highest shell is used). Provenance only.
  std::vector<float> fodfIgnoredShellBValues;
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

// Output of DwiShellSelectionStage: frame indices into the DWI series / bValues /
// gradientDirections, the chosen (highest-mean-b) shell, and the b-values of shells left out.
struct DwiShellSelection
{
  std::vector<size_t> b0Frames;
  std::vector<size_t> shellFrames;
  float shellBValue = 0.0f;
  std::vector<float> ignoredShellBValues;
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
  std::shared_ptr<const VolumeSeriesData> dwiSeries; // cached by DWIInputValidationStage; never copied to the result

  // calculated
  std::vector<float> bValues;
  std::vector<glm::vec3> gradientDirections;
  bool gradientMetadataValid = false;
  DwiShellSelection shellSelection;

  // output
  DTIVolumeChannels outputDtiChannels;
  std::shared_ptr<Mesh> outputSurfaceMesh;
  std::shared_ptr<Mesh> outputStreamlineMesh;
  std::vector<glm::vec3> outputSeedPoints;
  std::shared_ptr<const SHVolume> outputShVolume;
  FodfResponseFunction outputFodfResponse;
  std::shared_ptr<const SHVolume> outputFodfShVolume;
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
 *        reconstructed output. The stage list is assembled by a model-specific preprocessor
 *        (MriToDtiPreprocessor, MriToFodfPreprocessor); all models share one context/result shape.
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