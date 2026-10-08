#include "DtiVolumeScene.h"
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

/**
 * @brief Construct a new Dti Volume Scene:: Dti Volume Scene object
 * 
 */
DtiVolumeScene::DtiVolumeScene()
    : dtiVolume(nullptr)
{
  ClearGameObjects();
  ClearVolumes();

  tractographySettingsInspectable = std::make_shared<MriTractographySettings>();
  currentRequest.tractographySettings = tractographySettingsInspectable;
  currentRequest.dwiVolumePath = "assets/volumes/dwi/human/HARDI150_hdbet_masked4d.nii.gz";
  currentRequest.bvalPath = "assets/volumes/dwi/human/HARDI150.bval";
  currentRequest.bvecPath = "assets/volumes/dwi/human/HARDI150.bvec";
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
  currentRequest.tractographySettings = tractographySettingsInspectable;
  currentRequest.dwiVolumePath = dwiVolumePath;
  currentRequest.bvalPath = bvalPath;
  currentRequest.bvecPath = bvecPath;

  return ReloadDataset();
}

bool DtiVolumeScene::ReloadDataset()
{
  lastLoadError.clear();

  try
  {
    // Run preprocessing pipeline
    MriPreprocessingResult result = preprocessor.Process(currentRequest);

    // Check if preprocessing succeeded
    if (result.report.sourceVolumePath.empty())
    {
      lastLoadError = "No suitable volume found in dataset";
      return false;
    }

    if (!ApplyPreprocessingResult(result))
    {
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

void DtiVolumeScene::ClearProcessedScene()
{
  dtiVolume.reset();
  brainSurfaceObject.reset();
  streamlineObject.reset();
  ClearGameObjects();
  ClearVolumes();
}

bool DtiVolumeScene::ApplyPreprocessingResult(const MriPreprocessingResult &result)
{
  const int previousRenderMode = dtiVolume ? dtiVolume->GetSelectedRenderModeIndex() : 0;
  const int previousChannel = dtiVolume ? dtiVolume->GetSelectedChannelIndex() : 0;

  ClearProcessedScene();
  RebuildInspectProviders();

  // Create DTI volume from processed channels with tensor-eigenvector shader
  std::shared_ptr<Shader> volumeShader = std::make_shared<Shader>(
      "dti_volume_main_shader",
      "shaders/volume/volume_vertex.glsl",
      "shaders/dti_fragment_shaders/volume_dti_tensor_fragment.glsl");
  (*volumeShader)["shader.sliceZ"] = 0.5f;
  (*volumeShader)["shader.density"] = 1.0f;

  dtiVolume = std::make_shared<DTIVolume>("dti_volume_main", result.dtiChannels, volumeShader);
  dtiVolume->SetRotation(glm::vec3(-90.0f / 180.0f * glm::pi<float>(), 0.0f, 0.0f));
  dtiVolume->SetSelectedRenderModeIndex(previousRenderMode);
  dtiVolume->SetSelectedChannelIndex(previousChannel);

  AddVolume(dtiVolume);

  // Register shaders for hot reload tracking
  dtiVolume->RegisterShadersWithScene(this);

  // FA surface mesh
  if (result.surfaceMesh)
  {
    std::shared_ptr<Shader> meshShader = std::make_shared<Shader>(
        "dti_brain_surface_shader",
        "shaders/vertex.glsl",
      "shaders/pbr_fragment.glsl");
    this->RegisterShader("dti_brain_surface_shader", meshShader);

    std::shared_ptr<Material> meshMaterial = std::make_shared<Material>(meshShader);
    meshMaterial->SetBaseColor(glm::vec3(0.62f, 0.62f, 0.62f));
    meshMaterial->SetRoughness(0.32f);
    meshMaterial->SetMetallic(0.0f);

    result.surfaceMesh->SetMaterial(meshMaterial);
    brainSurfaceObject = std::make_shared<GameObject>("dti_brain_surface");
    brainSurfaceObject->AddMesh(result.surfaceMesh);
    brainSurfaceObject->SetRotation(glm::vec3(-90.0f / 180.0f * glm::pi<float>(), 0.0f, 0.0f));
    AddGameObject(brainSurfaceObject);
  }

  if (result.streamlineMesh)
  {
    std::shared_ptr<Shader> streamlineShader = std::make_shared<Shader>(
        "dti_streamline_shader",
      "shaders/streamlines/streamline_vertex.glsl",
      "shaders/streamlines/streamline_fragment.glsl");

    this->RegisterShader("dti_streamline_shader", streamlineShader);
    
    std::shared_ptr<Material> streamlineMaterial = std::make_shared<Material>(streamlineShader);

    result.streamlineMesh->SetMaterial(streamlineMaterial);
    streamlineObject = std::make_shared<GameObject>("dti_streamlines");
    streamlineObject->AddMesh(result.streamlineMesh);
    streamlineObject->SetRotation(glm::vec3(-90.0f / 180.0f * glm::pi<float>(), 0.0f, 0.0f));
    AddGameObject(streamlineObject);
  }

  RebuildInspectProviders();
  return true;
}

/**
 * @brief Update the scene, 
 *  applying a slow rotation to the brain surface and streamlines for better visualization of the 3D structure.
 * 
 * @param deltaTime 
 */
void DtiVolumeScene::Update(float deltaTime)
{
  if (datasetReloadRequested)
  {
    datasetReloadRequested = false;
    const bool reloaded = reloadCallback ? reloadCallback() : ReloadDataset();
    if (!reloaded)
    {
      std::cout << "DTI dataset reload failed: " << lastLoadError << "\n";
    }
  }

  Scene::Update(deltaTime);

  if (rotationEnabled)
  {
    const glm::vec3 rotationDelta(0.0f, 0.0f, deltaTime * rotationSpeed);
    if (streamlineObject)
    {
      streamlineObject->SetRotation(streamlineObject->GetRotation() + rotationDelta);
    }
    if (brainSurfaceObject)
    {
      brainSurfaceObject->SetRotation(brainSurfaceObject->GetRotation() + rotationDelta);
    }
    if (dtiVolume)
    {
      dtiVolume->SetRotation(dtiVolume->GetRotation() + rotationDelta);
    }
  }
}

/**
 * @brief Create inspect widgets for controlling scene parameters such as rotation.
 * 
 * @return engine inspection fields
 */
std::vector<InspectFieldPtr> DtiVolumeScene::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields = Scene::GetInspectFields();

  const auto addFileField = [&fields](const std::string& id, const std::string& label, std::string* path)
  {
    fields.push_back(MakeInspectField(
        id, label, "Preprocessing", InspectFieldType::File, *path,
        [path]() -> InspectValue { return *path; },
        [path](const InspectValue& value)
        {
          if (const auto* selectedPath = std::get_if<std::string>(&value)) *path = *selectedPath;
        }));
  };
  addFileField("dwiVolumePath", "DWI Volume", &currentRequest.dwiVolumePath);
  addFileField("bvalPath", "B-Values", &currentRequest.bvalPath);
  addFileField("bvecPath", "B-Vectors", &currentRequest.bvecPath);

  const std::vector<InspectFieldPtr> tractographyFields = tractographySettingsInspectable
      ? tractographySettingsInspectable->GetInspectFields()
      : std::vector<InspectFieldPtr>{};
  fields.insert(fields.end(), tractographyFields.begin(), tractographyFields.end());

  fields.push_back(MakeInspectField(
      "rerunPreprocessing", dtiVolume ? "Rerun preprocessing" : "Load DTI dataset", "Preprocessing", InspectFieldType::Action,
      std::monostate{}, {},
      [this](const InspectValue&) { datasetReloadRequested = true; }));

  // Rotation controls
  fields.push_back(MakeInspectField(
      "rotationEnabled", "Enabled", "Rotation", InspectFieldType::Boolean, rotationEnabled,
      [this]() -> InspectValue { return rotationEnabled; },
      [this](const InspectValue& value)
      {
        if (const auto* enabled = std::get_if<bool>(&value)) rotationEnabled = *enabled;
      }));

  auto rotationSpeedField = MakeInspectField(
      "rotationSpeed", "Speed", "Rotation", InspectFieldType::Number, static_cast<double>(rotationSpeed),
      [this]() -> InspectValue { return static_cast<double>(rotationSpeed); },
      [this](const InspectValue& value)
      {
        if (const auto* speed = std::get_if<double>(&value)) rotationSpeed = static_cast<float>(*speed);
      });
  rotationSpeedField->minimum = 0.0;
  rotationSpeedField->maximum = 5.0;
  rotationSpeedField->step = 0.01;
  fields.push_back(rotationSpeedField);

  return fields;
}
