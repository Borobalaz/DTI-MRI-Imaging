#pragma once

#include "scene/Scene.h"
#include "Preprocessing/MriFodfSettings.h"
#include "Preprocessing/MriFodfTractographySettings.h"
#include "Preprocessing/MriTractographySettings.h"
#include "scene/volume/DTIVolume.h"
#include "scene/volume/FodfVolume.h"
#include "Preprocessing/preprocessors/MriToDmriPreprocessor.h"
#include "foundation/threading/BackgroundTask.h"
#include "foundation/threading/SharedProgress.h"
#include "SliceController.h"

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
  // Run/Load only: result's fODF maps packed on the worker; empty when the result has none (or
  // packing failed, in which case result->fodfMaps was dropped too).
  std::optional<FodfVolumeTextureData> fodfVolumeTextures;
  MriPreprocessingRequest appliedRequest;       // Run: the request/settings snapshot the pipeline ran with
  std::string loadedPreprocessorType;           // Load only: decides how loadedParametersJson is applied
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
  MriFodfTractographySettings &GetFodfTractographySettings() { return *fodfTractographySettingsInspectable; }
  std::string GetLastLoadError() const { return lastLoadError; }

  void Update(float deltaTime) override;
  // Adds the slice plane's clipPlane frame uniforms (Cut Geometry) to the base scene uniforms.
  void Apply(IUniformSink &shader) const override;

  std::vector<InspectFieldPtr> GetDashboardFields() override;
  
private:
  using DatasetJob = BackgroundTask<DatasetJobOutcome>;
  // Which of the DTI/fODF object pairs (volume, streamlines, glyphs) the scene shows.
  enum class Representation { Dti, Fodf };

  void ClearProcessedScene();
  void ApplyPreprocessingResult(MriPreprocessingResult result,
                                DTIVolumeTextureData volumeTextures,
                                std::optional<FodfVolumeTextureData> fodfVolumeTextures,
                                const MriPreprocessingRequest& appliedRequest);
  std::shared_ptr<FodfVolume> CreateFodfVolume(FodfVolumeTextureData fodfVolumeTextures,
                                               int renderModeIndex,
                                               int channelIndex) const;
  std::shared_ptr<GameObject> CreateStreamlineObject(const std::string& id, const std::shared_ptr<Mesh>& mesh) const;
  std::shared_ptr<SHGlyphField> CreateGlyphField(const std::string& id,
                                                 const std::shared_ptr<const SHVolume>& volume,
                                                 SHGlyphDisplayMode displayMode) const;
  void ApplyLoadedParameters(const std::string& preprocessorType, const std::string& parametersJson);

  // Representation state. Before any dataset is applied fODF counts as available, so the
  // preference can be pre-set; afterwards it is available unless fodfUnavailableReason says why not.
  bool IsFodfAvailable() const;
  Representation GetEffectiveRepresentation() const;
  // Shows the effective side of each pair and hides the other, overriding manual visibility; when
  // the effective side of a pair is missing (legacy datasets) the other side stays visible.
  void ApplyRepresentationVisibility();
  std::vector<InspectFieldPtr> GetRepresentationDashboardFields();

  // Inspector field metadata (offset range/step, glyph Stride readOnly) is frozen at fetch time;
  // re-fetch it by bumping the provider generation when any of its inputs changed.
  void RebuildInspectProvidersIfLayoutChanged();

  // Shaders compiled once (GL thread) and reused by every apply, so a rerun compiles nothing.
  // Their stored uniforms (inspector tweaks) therefore persist across reruns.
  struct DtiShaderCache
  {
    std::vector<DTIVolume::RenderMode> volumeRenderModes;
    std::vector<FodfVolume::RenderMode> fodfVolumeRenderModes;
    std::shared_ptr<Shader> brainSurface;
    std::shared_ptr<Shader> streamline; // DTI and fODF streamlines
    std::shared_ptr<Shader> seedPoint;
    std::shared_ptr<Shader> shGlyph;    // tensor-ODF and fODF glyphs
    std::shared_ptr<Shader> slicePlaneUnlit; // outline + fill (SliceController)
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
  std::shared_ptr<MriFodfSettings> fodfSettingsInspectable;
  std::shared_ptr<MriFodfTractographySettings> fodfTractographySettingsInspectable;
  std::shared_ptr<DTIVolume> dtiVolume;
  std::shared_ptr<FodfVolume> fodfVolume;
  std::shared_ptr<GameObject> brainSurfaceObject;
  std::shared_ptr<GameObject> dtiStreamlineObject;
  std::shared_ptr<GameObject> fodfStreamlineObject;
  std::shared_ptr<GameObject> seedPointsObject;
  std::shared_ptr<SHGlyphField> tensorOdfGlyphField;
  std::shared_ptr<SHGlyphField> fodfGlyphField;

  // Scene-wide slicer (one plane in dataset space) and its drawable. Created and registered once
  // in the ctor, never replaced, so the plane persists across reruns and loads.
  std::shared_ptr<SliceController> sliceController;

  // Last inspector-metadata inputs published via RebuildInspectProviders.
  float publishedOffsetLimit = -1.0f;
  float publishedOffsetStep = -1.0f;
  int publishedTensorOdfGlyphDisplayMode = -1; // -1: no glyph field
  int publishedFodfGlyphDisplayMode = -1;      // -1: no glyph field

  // Persists across reruns and loads; the effective one falls back to DTI when fODF is unavailable.
  Representation selectedRepresentation = Representation::Fodf;
  // Set by the Representation dropdown; Update() applies the visibility and refreshes the eye icons.
  bool representationVisibilityDirty = false;
  // Why the applied dataset has no fODF objects; empty when it has some or none is applied yet.
  std::string fodfUnavailableReason;

  // Shared with running jobs; its stages are const and stateless.
  std::shared_ptr<const MriToDmriPreprocessor> preprocessor;
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

  bool rotationEnabled = false;
  float rotationSpeed = 0.2f;
};
