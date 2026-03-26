#include "Scene/DtiVolumeScene.h"
#include "Shader.h"
#include "Skybox.h"
#include "PointLight.h"
#include "DirectionalLight.h"
#include "PerspectiveCamera.h"

#include <iostream>
#include <glm/glm.hpp>

DtiVolumeScene::DtiVolumeScene()
  : dtiVolume(nullptr)
{
  SetupDefaultVisualization();
}

DtiVolumeScene::~DtiVolumeScene() = default;

/**
 * @brief Load a DTI dataset and initialize the scene for visualization
 * 
 * @param datasetRootPath The root path to the dataset
 * @param subjectId The ID of the subject
 * @param sessionId The ID of the session
 * @return true if the dataset was loaded successfully, false otherwise
 */
bool DtiVolumeScene::LoadDataset(
  const std::string& datasetRootPath,
  const std::string& subjectId,
  const std::string& sessionId)
{
  lastLoadError.clear();

  try
  {
    // Configure preprocessing request
    MriPreprocessingRequest request;
    request.datasetRootPath = datasetRootPath;
    request.preferredSubjectId = subjectId;
    request.preferredSessionId = sessionId;
    request.preferAnatomicalVolumes = true;
    request.generateAllChannels = true;

    // Run preprocessing pipeline
    MriPreprocessingResult result = preprocessor.Process(request);

    // Check if preprocessing succeeded
    if (result.report.sourceVolumePath.empty())
    {
      lastLoadError = "No suitable volume found in dataset";
      return false;
    }

    std::cout << "✓ Loaded: " << result.report.sourceVolumePath << std::endl;
    std::cout << "  Executed stages:\n";
    for (const auto& stage : result.report.executedStages)
    {
      std::cout << "    • " << stage << "\n";
    }

    if (!result.report.warnings.empty())
    {
      std::cout << "  Warnings:\n";
      for (const auto& warning : result.report.warnings)
      {
        std::cout << "    ⚠ " << warning << "\n";
      }
    }

    // Create DTI volume from processed channels with volume shader
    std::shared_ptr<Shader> volumeShader = std::make_shared<Shader>(
        "shaders/volume_vertex.glsl",
        "shaders/volume_fragment.glsl"
      );
    
    dtiVolume = std::make_shared<DTIVolume>(result.channels, volumeShader);
    
    // Set initial rendering parameters
    dtiVolume->SetActiveMetric(0);  // 0=FA, 1=MD, 2=AD, 3=RD
    
    // Add to scene for rendering
    ClearVolumes();  // Remove any previous volumes
    AddVolume(dtiVolume);

    return true;
  }
  catch (const std::exception& ex)
  {
    lastLoadError = std::string("Exception during preprocessing: ") + ex.what();
    std::cerr << lastLoadError << std::endl;
    return false;
  }
  catch (...)
  {
    lastLoadError = "Unknown error during preprocessing";
    std::cerr << lastLoadError << std::endl;
    return false;
  }
}

/**
 * @brief Set the channel to visualize (0=FA, 1=MD, 2=AD, 3=RD)
 * 
 * @param metricIndex 
 */
void DtiVolumeScene::SetActiveMetric(int metricIndex)
{
  if (dtiVolume)
  {
    dtiVolume->SetActiveMetric(metricIndex);
  }
}

/**
 * @brief Set the threshold for volume rendering. Voxels with metric values below this threshold will be hidden.
 * 
 * @param threshold 
 */
void DtiVolumeScene::SetThreshold(float threshold)
{
  if (dtiVolume)
  {
    // Note: threshold is exposed through the UI/inspectable system
    // To set it programmatically, you would need to add public accessors to DTIVolume
    // For now, this is a placeholder for future enhancement
  }
}

/**
 * @brief Set the opacity for volume rendering. Voxels with metric values below this threshold will be hidden.
 * 
 * @param opacity 
 */
void DtiVolumeScene::SetOpacity(float opacity)
{
  if (dtiVolume)
  {
    // Note: opacity is exposed through the UI/inspectable system
    // To set it programmatically, you would need to add public accessors to DTIVolume
    // For now, this is a placeholder for future enhancement
  }
}

/**
 * @brief Get the DTI volume associated with the scene
 * 
 * @return std::shared_ptr<DTIVolume> 
 */
std::shared_ptr<DTIVolume> DtiVolumeScene::GetDtiVolume() const
{
  return dtiVolume;
}

/**
 * @brief Set up default visualization parameters for the DTI volume
 */
void DtiVolumeScene::SetupDefaultVisualization()
{
  // Scene constructor already sets up default camera and lighting
  // This method can be extended for DTI-specific visualization parameters
  std::cout << "DTI Volume Scene initialized with default visualization settings.\n";
}
