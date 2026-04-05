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

  /**
   * @brief Load and process DTI inputs into tensor channels for rendering.
   */
  bool LoadDataset(
    const std::string& dwiVolumePathOverride,
    const std::string& bvalPathOverride,
    const std::string& bvecPathOverride);

  /**
   * @brief Set the active DTI metric to display.
   * 
   * @param metric The metric to display (0=FA, 1=MD, 2=AD, 3=RD)
   */
  void SetActiveMetric(int metricIndex);

  /**
   * @brief Set the threshold for volume rendering.
   * 
   * Voxels with metric values below this threshold are hidden.
   * 
   * @param threshold Threshold value (0.0 to 1.0)
   */
  void SetThreshold(float threshold);

  /**
   * @brief Set the opacity of the volume.
   * 
   * @param opacity Opacity value (0.0 = transparent, 1.0 = opaque)
   */
  void SetOpacity(float opacity);

  /**
   * @brief Get the currently loaded DTI volume.
   * 
   * @return Shared pointer to the DTI volume, or nullptr if not loaded
   */
  std::shared_ptr<DTIVolume> GetDtiVolume() const;

  /**
   * @brief Get the last error message from preprocessing.
   * 
   * @return Error message, or empty string if no error
   */
  std::string GetLastLoadError() const { return lastLoadError; }

private:
  std::shared_ptr<DTIVolume> dtiVolume;
  MriToDtiPreprocessor preprocessor;
  std::string lastLoadError;
};
