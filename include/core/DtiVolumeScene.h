#pragma once

#include "Scene/Scene.h"
#include "Volume/DTIVolume.h"
#include "Preprocessing/MriToDtiPreprocessor.h"

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
 */
class DtiVolumeScene : public Scene
{
public:
  DtiVolumeScene();
  
  bool LoadDataset(
    const std::string& dwiVolumePathOverride,
    const std::string& bvalPathOverride,
    const std::string& bvecPathOverride);
  std::string GetLastLoadError() const { return lastLoadError; }

  void Update(float deltaTime) override;

  std::vector<std::shared_ptr<IInspectWidget>> GetInspectFields() override;
  
private:
  std::shared_ptr<DTIVolume> dtiVolume;
  std::shared_ptr<GameObject> brainSurfaceObject;
  std::shared_ptr<GameObject> streamlineObject;
  MriToDtiPreprocessor preprocessor;
  std::string lastLoadError;

  bool rotationEnabled = true;
  float rotationSpeed = 0.2f;
};
