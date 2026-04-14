#include "Scene/DtiVolumeScene.h"
#include "Shader.h"
#include "Skybox.h"
#include "PointLight.h"
#include "DirectionalLight.h"
#include "PerspectiveCamera.h"
#include "Material.h"
#include "Mesh.h"
#include "GameObject.h"

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

    // Print preprocessing report
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
        "shaders/dti_fragment_shaders/volume_dti_tensor_fragment.glsl"
      );
    (*volumeShader)["shader.sliceZ"] = 0.5f;
    volumeShader->SetUniformUiFloatRange("shader.sliceZ", 0.0f, 1.0f, 0.001f);
    (*volumeShader)["shader.density"] = 1.0f;
    volumeShader->SetUniformUiFloatRange("shader.density", 0.0f, 1.0f, 0.0001f);
    
    dtiVolume = std::make_shared<DTIVolume>(result.channels, volumeShader);
    dtiVolume->SetRotation(glm::vec3(-90.0f / 180.0f * glm::pi<float>(), 0.0f, 0.0f));

    // Add to scene for rendering
    ClearVolumes();
    AddVolume(dtiVolume);

    // Register shaders for hot reload tracking
    dtiVolume->RegisterShadersWithScene(this);

    ClearGameObjects();
    if (result.surfaceMesh)
    {
      std::shared_ptr<Shader> meshShader = std::make_shared<Shader>(
          "shaders/vertex.glsl",
          "shaders/fragment.glsl");

      std::shared_ptr<Material> meshMaterial = std::make_shared<Material>(meshShader);
      meshMaterial->SetAmbientColor(glm::vec3(0.35f, 0.35f, 0.35f));
      meshMaterial->SetDiffuseColor(glm::vec3(0.62f, 0.62f, 0.62f));
      meshMaterial->SetSpecularColor(glm::vec3(0.08f, 0.08f, 0.08f));
      meshMaterial->SetShininess(18.0f);

      result.surfaceMesh->SetMaterial(meshMaterial);
      std::shared_ptr<GameObject> brainSurfaceObject = std::make_shared<GameObject>();
      brainSurfaceObject->AddMesh(result.surfaceMesh);
      brainSurfaceObject->SetRotation(glm::vec3(-90.0f / 180.0f * glm::pi<float>(), 0.0f, 0.0f));
      AddGameObject(brainSurfaceObject);
    }
    else
    {
      result.report.warnings.push_back("No brain surface mesh was generated for DTI scene rendering.");
    }

    if (result.streamlineMesh)
    {
      std::shared_ptr<Shader> streamlineShader = std::make_shared<Shader>(
          "shaders/streamline_vertex.glsl",
          "shaders/streamline_fragment.glsl");

      std::shared_ptr<Material> streamlineMaterial = std::make_shared<Material>(streamlineShader);
      streamlineMaterial->SetAmbientColor(glm::vec3(1.0f, 0.58f, 0.16f));
      streamlineMaterial->SetDiffuseColor(glm::vec3(1.0f, 0.58f, 0.16f));
      streamlineMaterial->SetSpecularColor(glm::vec3(0.0f, 0.0f, 0.0f));
      streamlineMaterial->SetShininess(1.0f);

      result.streamlineMesh->SetMaterial(streamlineMaterial);
      std::shared_ptr<GameObject> streamlineObject = std::make_shared<GameObject>();
      streamlineObject->AddMesh(result.streamlineMesh);
      streamlineObject->SetRotation(glm::vec3(-90.0f / 180.0f * glm::pi<float>(), 0.0f, 0.0f));
      AddGameObject(streamlineObject);
    }
    else
    {
      result.report.warnings.push_back("No fiber streamlines were generated for DTI scene rendering.");
    }

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
