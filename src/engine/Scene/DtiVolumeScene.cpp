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
}

/**
 * @brief Load a DTI dataset and initialize the scene for visualization
 * 
 * @param dwiVolumePath The path to the DWI volume
 * @param bvalPath The path to the b-values file
 * @param bvecPath The path to the b-vectors file
 * @return true if the dataset was loaded successfully, false otherwise
 */
bool DtiVolumeScene::LoadDataset(
  const std::string& dwiVolumePath,
  const std::string& bvalPath,
  const std::string& bvecPath)
{
  lastLoadError.clear();

  try
  {
    // Configure preprocessing request
    MriPreprocessingRequest request;
    request.dwiVolumePath = dwiVolumePath;
    request.bvalPath = bvalPath;
    request.bvecPath = bvecPath;

    // Run preprocessing pipeline
    MriPreprocessingResult result = preprocessor.Process(request);

    // Check if preprocessing succeeded
    if (result.report.sourceVolumePath.empty())
    {
      lastLoadError = "No suitable volume found in dataset";
      return false;
    }

    std::cout << "Loaded: " << result.report.sourceVolumePath << std::endl;
    std::cout << "Executed stages:\n";
    for (const auto& stage : result.report.executedStages)
    {
      std::cout << "    - " << stage << "\n";
    }

    if (!result.report.warnings.empty())
    {
      std::cout << "  Warnings:\n";
      for (const auto& warning : result.report.warnings)
      {
        std::cout << "    [!] " << warning << "\n";
      }
    }

    // Create DTI volume from processed channels with tensor-eigenvector shader
    std::shared_ptr<Shader> volumeShader = std::make_shared<Shader>(
        "shaders/volume_vertex.glsl",
        "shaders/volume_dti_tensor_fragment.glsl"
      );
    (*volumeShader)["shader.sliceZ"] = 0.5f;
    volumeShader->SetUniformUiFloatRange("shader.sliceZ", 0.0f, 1.0f, 0.001f);
    (*volumeShader)["shader.density"] = 400.0f;
    volumeShader->SetUniformUiFloatRange("shader.density", 0.0f, 2000.0f, 1.0f);
    
    dtiVolume = std::make_shared<DTIVolume>(result.channels, volumeShader);
        
    // Add to scene for rendering
    ClearVolumes();
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
