#include "core/DtiVolumeScene.h"
#include "Shader.h"
#include "Texture/Skybox.h"
#include "Light/PointLight.h"
#include "Light/DirectionalLight.h"
#include "Camera/PerspectiveCamera.h"
#include "Material.h"
#include "Mesh.h"
#include "GameObject.h"

#include <iostream>
#include <glm/glm.hpp>

#include "ui/widgets/inspect_fields/InspectNumberFieldWidget.h"
#include "ui/widgets/inspect_fields/InspectCheckboxFieldWidget.h"

/**
 * @brief Construct a new Dti Volume Scene:: Dti Volume Scene object
 * 
 */
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
    const std::string &dwiVolumePath,
    const std::string &bvalPath,
    const std::string &bvecPath)
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
    for (const auto &stage : result.report.executedStages)
    {
      std::cout << "    - " << stage << "\n";
    }

    if (!result.report.warnings.empty())
    {
      std::cout << "  Warnings:\n";
      for (const auto &warning : result.report.warnings)
      {
        std::cout << "    [!] " << warning << "\n";
      }
    }

    // Create DTI volume from processed channels with tensor-eigenvector shader
    std::shared_ptr<Shader> volumeShader = std::make_shared<Shader>(
        "dti_volume_main_shader",
        "shaders/volume_vertex.glsl",
        "shaders/dti_fragment_shaders/volume_dti_tensor_fragment.glsl");
    (*volumeShader)["shader.sliceZ"] = 0.5f;
    (*volumeShader)["shader.density"] = 1.0f;

    dtiVolume = std::make_shared<DTIVolume>("dti_volume_main", result.channels, volumeShader);
    dtiVolume->SetRotation(glm::vec3(-90.0f / 180.0f * glm::pi<float>(), 0.0f, 0.0f));

    AddVolume(dtiVolume);
    AddInspectProvider(dtiVolume);

    // Register shaders for hot reload tracking
    dtiVolume->RegisterShadersWithScene(this);

    // FA surface mesh
    if (result.surfaceMesh)
    {
      std::shared_ptr<Shader> meshShader = std::make_shared<Shader>(
          "dti_brain_surface_shader",
          "shaders/vertex.glsl",
          "shaders/fragment.glsl");

      std::shared_ptr<Material> meshMaterial = std::make_shared<Material>(meshShader);
      meshMaterial->SetAmbientColor(glm::vec3(0.35f, 0.35f, 0.35f));
      meshMaterial->SetDiffuseColor(glm::vec3(0.62f, 0.62f, 0.62f));
      meshMaterial->SetSpecularColor(glm::vec3(0.08f, 0.08f, 0.08f));
      meshMaterial->SetShininess(18.0f);

      result.surfaceMesh->SetMaterial(meshMaterial);
      brainSurfaceObject = std::make_shared<GameObject>("dti_brain_surface");
      brainSurfaceObject->AddMesh(result.surfaceMesh);
      brainSurfaceObject->SetRotation(glm::vec3(-90.0f / 180.0f * glm::pi<float>(), 0.0f, 0.0f));
      AddGameObject(brainSurfaceObject);
      AddInspectProvider(brainSurfaceObject);
    }
    else
    {
      result.report.warnings.push_back("No brain surface mesh was generated for DTI scene rendering.");
    }

    if (result.streamlineMesh)
    {
      std::shared_ptr<Shader> streamlineShader = std::make_shared<Shader>(
          "dti_streamline_shader",
          "shaders/streamline_vertex.glsl",
          "shaders/streamline_fragment.glsl");

      std::shared_ptr<Material> streamlineMaterial = std::make_shared<Material>(streamlineShader);
      streamlineMaterial->SetAmbientColor(glm::vec3(1.0f, 0.58f, 0.16f));
      streamlineMaterial->SetDiffuseColor(glm::vec3(1.0f, 0.58f, 0.16f));
      streamlineMaterial->SetSpecularColor(glm::vec3(0.0f, 0.0f, 0.0f));
      streamlineMaterial->SetShininess(1.0f);

      result.streamlineMesh->SetMaterial(streamlineMaterial);
      streamlineObject = std::make_shared<GameObject>("dti_streamlines");
      streamlineObject->AddMesh(result.streamlineMesh);
      streamlineObject->SetRotation(glm::vec3(-90.0f / 180.0f * glm::pi<float>(), 0.0f, 0.0f));
      AddGameObject(streamlineObject);
      AddInspectProvider(streamlineObject);
    }
    else
    {
      result.report.warnings.push_back("No fiber streamlines were generated for DTI scene rendering.");
    }

    return true;
  }
  catch (const std::exception &ex)
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
 * @brief Update the scene, 
 *  applying a slow rotation to the brain surface and streamlines for better visualization of the 3D structure.
 * 
 * @param deltaTime 
 */
void DtiVolumeScene::Update(float deltaTime)
{
  Scene::Update(deltaTime);

  if (rotationEnabled)
  {
    streamlineObject->SetRotation(streamlineObject->GetRotation() + glm::vec3(0.0f, 0.0f, deltaTime * rotationSpeed));
    brainSurfaceObject->SetRotation(brainSurfaceObject->GetRotation() + glm::vec3(0.0f, 0.0f, deltaTime * rotationSpeed));
    dtiVolume->SetRotation(dtiVolume->GetRotation() + glm::vec3(0.0f, 0.0f, deltaTime * rotationSpeed));
  }
}

/**
 * @brief Create inspect widgets for controlling scene parameters such as rotation.
 * 
 * @return std::vector<std::shared_ptr<IInspectWidget>> 
 */
std::vector<std::shared_ptr<IInspectWidget>> DtiVolumeScene::GetInspectFields()
{
  std::vector<std::shared_ptr<IInspectWidget>> fields = Scene::GetInspectFields(); // Get base scene fields

  auto rotationEnabledField = std::make_shared<InspectCheckboxFieldWidget>(
      "rotationEnabled",
      "Enabled",
      "Rotation");
  rotationEnabledField->SetValue(QVariant(rotationEnabled));
  rotationEnabledField->valueChangedCallback = [this](const QVariant &value)
  {
    rotationEnabled = value.toBool();
  };
  fields.push_back(rotationEnabledField);

  auto rotationSpeedField = std::make_shared<InspectNumberFieldWidget>(
      "rotationSpeed",
      "Speed",
      "Rotation",
      [this]()
      { return (double)rotationSpeed; },
      [this](double newValue)
      { rotationSpeed = newValue; },
      0.0, 5.0);
  fields.push_back(rotationSpeedField);

  return fields;
}
