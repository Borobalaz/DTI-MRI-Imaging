#pragma once

#include "Scene/Scene.h"
#include "Preprocessing/MriTractographySettings.h"
#include "Volume/DTIVolume.h"
#include "Preprocessing/preprocessors/MriToDtiPreprocessor.h"

#include <functional>
#include <memory>
#include <string>

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
  
  bool LoadDataset(
    const std::string& dwiVolumePathOverride,
    const std::string& bvalPathOverride,
    const std::string& bvecPathOverride);
  bool ReloadDataset();
  MriPreprocessingRequest GetCurrentRequest() const { return currentRequest; }
  void SetReloadCallback(std::function<bool()> callback) { reloadCallback = std::move(callback); }
  MriTractographySettings &GetTractographySettings() { return *tractographySettingsInspectable; }
  std::string GetLastLoadError() const { return lastLoadError; }

  void Update(float deltaTime) override;

  std::vector<InspectFieldPtr> GetInspectFields() override;
  
private:
  void ClearProcessedScene();
  bool ApplyPreprocessingResult(MriPreprocessingResult result);
  bool SaveCurrentDataset(const std::string& outputDirectory);
  bool LoadSavedDataset(const std::string& metadataJsonPath);

  MriPreprocessingRequest currentRequest;
  std::shared_ptr<MriTractographySettings> tractographySettingsInspectable;
  std::function<bool()> reloadCallback;
  std::shared_ptr<DTIVolume> dtiVolume;
  std::shared_ptr<GameObject> brainSurfaceObject;
  std::shared_ptr<GameObject> streamlineObject;
  MriToDtiPreprocessor preprocessor;
  std::string lastLoadError;
  bool datasetReloadRequested = false;

  // Cache of the most recently applied preprocessing result, so "Save current dataset" can
  // persist it without re-running the (potentially slow) preprocessing pipeline.
  MriPreprocessingResult lastPreprocessingResult;
  bool hasLastPreprocessingResult = false;
  std::string pendingSaveOutputDirectory;
  std::string pendingLoadMetadataPath;
  bool datasetSaveRequested = false;
  bool datasetLoadRequested = false;
  std::string lastSaveError;

  bool rotationEnabled = true;
  float rotationSpeed = 0.2f;
};
