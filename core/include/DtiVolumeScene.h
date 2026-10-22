#pragma once

#include "Scene/Scene.h"
#include "Preprocessing/MriTractographySettings.h"
#include "Volume/DTIVolume.h"
#include "Preprocessing/preprocessors/MriToDtiPreprocessor.h"
#include "Threading/BackgroundTask.h"
#include "Threading/SharedProgress.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

enum class DatasetJobKind
{
  Run,
  Load,
  Save
};

/**
 * @brief What a background dataset job hands back to the scene's Update() thread. Holds only
 *        CPU data (meshes and volume textures are GL-free until first drawn), so it may be built
 *        on the worker.
 */
struct DatasetJobOutcome
{
  DatasetJobKind kind = DatasetJobKind::Run;
  std::optional<MriPreprocessingResult> result; // Run/Load only
  std::optional<DTIVolumeTextureData> volumeTextures; // Run/Load only: result's channels packed on the worker
  MriPreprocessingRequest appliedRequest;       // Run: the request/settings snapshot the pipeline ran with
  std::string loadedParametersJson;             // Load only: provenance applied to the scene at apply time
  std::string error;                            // non-empty when the job failed
  bool cancelled = false;
};

/**
 * @brief A specialized scene for viewing DTI (Diffusion Tensor Imaging) volumes.
 *
 * This scene loads MRI data from a neuroimaging dataset, processes it into DTI metrics
 * (FA, MD, AD, RD), and renders the selected metric as a 3D volume texture.
 *
 * Provides interactive controls for:
 * - Metric selection (FA, MD, AD, RD)
 * - Threshold adjustment
 * - Opacity control
 *
 * `DtiVolumeScene` is the concrete DTI-specific scene builder on top of the generic
 * `core/` preprocessing pipeline (`MriPreprocessingPipeline`). It intentionally depends on
 * engine-private `Scene`/`GameObject` types - a pre-existing exception to `core/`'s general
 * engine-independence, not addressed by this change.
 */
class DtiVolumeScene : public Scene
{
public:
  DtiVolumeScene();
  ~DtiVolumeScene() override;
  
  // Both queue an asynchronous preprocessing run; its result is applied by a later Update().
  void LoadDataset(
    const std::string& dwiVolumePathOverride,
    const std::string& bvalPathOverride,
    const std::string& bvecPathOverride);
  void ReloadDataset();
  MriPreprocessingRequest GetCurrentRequest() const { return currentRequest; }
  MriTractographySettings &GetTractographySettings() { return *tractographySettingsInspectable; }
  std::string GetLastLoadError() const { return lastLoadError; }

  void Update(float deltaTime) override;

  std::vector<InspectFieldPtr> GetInspectFields() override;
  
private:
  using DatasetJob = BackgroundTask<DatasetJobOutcome>;

  void ClearProcessedScene();
  void ApplyPreprocessingResult(MriPreprocessingResult result,
                                DTIVolumeTextureData volumeTextures,
                                const MriPreprocessingRequest& appliedRequest);

  // Shaders compiled once (GL thread) and reused by every apply, so a rerun compiles nothing.
  // Their stored uniforms (inspector tweaks) therefore persist across reruns.
  struct DtiShaderCache
  {
    std::vector<DTIVolume::RenderMode> volumeRenderModes;
    std::shared_ptr<Shader> brainSurface;
    std::shared_ptr<Shader> streamline;
    std::shared_ptr<Shader> seedPoint;
  };
  void EnsureShaderCache();

  // Background job orchestration. All of these run on the thread calling Update() (the GL
  // thread); only the work functions handed to DatasetJob run on the worker.
  void PollJobs();
  void RequestJob(DatasetJobKind kind);
  void CancelPreprocessingJobs();
  void RetireActiveJob();
  void StartPendingJob();
  std::unique_ptr<DatasetJob> CreateRunJob();
  std::unique_ptr<DatasetJob> CreateLoadJob();
  std::unique_ptr<DatasetJob> CreateSaveJob();
  void ApplyJobOutcome(DatasetJobOutcome outcome);
  void FailJob(DatasetJobKind kind, const std::string& error);
  void RefreshStatusText();
  std::string GetPreprocessingStatus() const;

  MriPreprocessingRequest currentRequest;
  std::shared_ptr<MriTractographySettings> tractographySettingsInspectable;
  std::shared_ptr<DTIVolume> dtiVolume;
  std::shared_ptr<GameObject> brainSurfaceObject;
  std::shared_ptr<GameObject> streamlineObject;
  std::shared_ptr<GameObject> seedPointsObject;
  // Shared with running jobs; its stages are const and stateless.
  std::shared_ptr<const MriToDtiPreprocessor> preprocessor;
  std::string lastLoadError;
  bool datasetReloadRequested = false;

  // Cache of the most recently applied preprocessing result plus the request/settings it was
  // produced with, so "Save current dataset" can persist it (with accurate provenance) on a
  // worker without re-running the (potentially slow) preprocessing pipeline.
  std::shared_ptr<const MriPreprocessingResult> lastPreprocessingResult;
  MriPreprocessingRequest lastAppliedRequest;
  std::string pendingSaveOutputDirectory;
  std::string pendingLoadMetadataPath;
  bool datasetSaveRequested = false;
  bool datasetLoadRequested = false;
  bool cancelRequested = false;
  std::string lastSaveError;

  // One job in flight at a time. A superseded Run/Load is cancelled and parked in retiringJob
  // until it notices, so Update() never blocks on a join; if that slot is still occupied the
  // cancelled job stays in activeJob (activeJobCancelled) and its outcome is discarded.
  std::unique_ptr<DatasetJob> activeJob;
  DatasetJobKind activeJobKind = DatasetJobKind::Run;
  bool activeJobCancelled = false;
  std::unique_ptr<DatasetJob> retiringJob;
  std::optional<DatasetJobKind> pendingJob;
  std::shared_ptr<SharedProgress> jobProgress;
  std::string statusText = "Ready";
  std::string statusNotice; // e.g. a refused Save, shown next to the running job's progress

  DtiShaderCache shaderCache;
  bool shaderCacheWarmed = false;

  bool rotationEnabled = true;
  float rotationSpeed = 0.2f;
};
