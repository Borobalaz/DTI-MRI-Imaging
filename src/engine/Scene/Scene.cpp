#include "Scene/Scene.h"
#include <algorithm>

#include "Mesh.h"
#include "ModelLoader.h"
#include "Skybox.h"
#include "Texture2D.h"
#include "FloatVolume.h"
#include "VolumeFileLoader.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <iostream>

#include <glm/gtc/constants.hpp>

namespace
{
  constexpr int kMaxLights = 16;

  VolumeData CreateSeedVolumeData(int width, int height, int depth)
  {
    VolumeData data(width, height, depth);
    for (float& voxel : data.GetVoxels())
    {
      voxel = 1.0f;
    }
    return data;
  }
}

/**
 * @brief Construct a new Scene:: Scene object
 * 
 */
Scene::Scene()
  : clearColor{1.0f, 1.0f, 1.0f, 1.0f},
    camera(std::make_shared<PerspectiveCamera>(45.0f, 800.0f / 600.0f, 0.1f, 100.0f)),
    matrixTestUniformValue(0.5f, 0.5f, 0.5f)
{

  // ------------- SHADERS -------------


  // ------------- MATERIALS -------------

  // ------------- GAME OBJECTS -------------
  //const std::shared_ptr<GameObject> importedModel =
  //  ModelLoader::LoadGameObject("assets/models/assasin.fbx", basicShader);
  //if (importedModel)
  //{
  //  importedModel->position = glm::vec3(0.0f, 0.0f, 0.0f);
  //  importedModel->scale = glm::vec3(1.0f, 1.0f, 1.0f);
  //  importedModel->rotation = glm::vec3(-glm::half_pi<float>(), 0.0f, -glm::half_pi<float>());
  //  gameObjects.push_back(importedModel);
  //}
  //else
  //{
  //  //std::cout << "Failed to load model\n";
  //}

  // ------------- LIGHTS -------------
  lights.push_back(std::make_shared<PointLight>(PointLight(
    glm::vec3(0.0f, 0.0f, 2.0f),
    glm::vec3(1.0f, 0.08f, 0.08f),
    glm::vec3(0.9f, 0.9f, 0.9f),
    glm::vec3(1.0f, 1.0f, 1.0f),
    1.0f,
    0.09f,
    0.032f
  )));

  lights.push_back(std::make_shared<DirectionalLight>(DirectionalLight(
    glm::vec3(-0.2f, -1.0f, -0.3f),
    glm::vec3(0.05f, 0.05f, 0.05f),
    glm::vec3(0.45f, 0.45f, 0.45f),
    glm::vec3(0.35f, 0.35f, 0.35f)
  )));

  // ------------- VOLUME -------------
  std::shared_ptr<Shader> mandelbulbVolumeShader = std::make_shared<Shader>(
    "shaders/volume_vertex.glsl",
    "shaders/mandelbulb_fragment.glsl"
  );

  std::shared_ptr<Volume> mandelbulbVolume = std::make_shared<FloatVolume>(
    CreateSeedVolumeData(8, 8, 8),
    mandelbulbVolumeShader
  );
  mandelbulbVolume->setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
  mandelbulbVolume->setScale(glm::vec3(1.5f, 1.5f, 1.5f));

  (*mandelbulbVolumeShader)["power"] = 8.0f;
  (*mandelbulbVolumeShader)["bailout"] = 8.0f;
  (*mandelbulbVolumeShader)["hitEpsilon"] = 0.0012f;
  (*mandelbulbVolumeShader)["maxDistance"] = 4.0f;
  (*mandelbulbVolumeShader)["stepScale"] = 0.75f;
  (*mandelbulbVolumeShader)["fractalScale"] = 3.2f;
  (*mandelbulbVolumeShader)["time"] = 0.0f;
  (*mandelbulbVolumeShader)["animationSpeed"] = 0.150f;
  (*mandelbulbVolumeShader)["maxSteps"] = 256;
  (*mandelbulbVolumeShader)["renderMode"] = 0;
  (*mandelbulbVolumeShader)["animateBulb"] = false;
  (*mandelbulbVolumeShader)["fractalOffset"] = glm::vec3(0.0f, 0.0f, 0.0f);
  (*mandelbulbVolumeShader)["baseColor"] = glm::vec3(0.95f, 0.55f, 0.3f);

  mandelbulbVolumeShader->SetUniformUiFloatRange("power", 0.0f, 30.0f, 0.001f);
  mandelbulbVolumeShader->SetUniformUiFloatRange("bailout", 2.0f, 16.0f, 0.1f);
  mandelbulbVolumeShader->SetUniformUiFloatRange("hitEpsilon", 0.03f, 0.5f, 0.001f);
  mandelbulbVolumeShader->SetUniformUiFloatRange("maxDistance", 0.5f, 8.0f, 0.05f);
  mandelbulbVolumeShader->SetUniformUiFloatRange("stepScale", 0.2f, 1.0f, 0.01f);
  mandelbulbVolumeShader->SetUniformUiFloatRange("fractalScale", 0.5f, 8.0f, 0.05f);
  mandelbulbVolumeShader->SetUniformUiFloatRange("animationSpeed", 0.001f, 1.0f, 0.001f);
  mandelbulbVolumeShader->SetUniformUiIntRange("maxSteps", 16, 512);
  mandelbulbVolumeShader->SetUniformUiIntRange("renderMode", 0, 1);

  volumes.push_back(mandelbulbVolume);
}

/**
 * @brief Initialize the scene, 
 *        enable depth testing, set clear color, set depth function, 
 *        enable seamless cubemap sampling
 * 
 */
void Scene::Init()
{
  glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

/**
 * @brief Game loop update function
 * 
 * @param deltaTime 
 */
void Scene::Update(float deltaTime)
{
  if (camera)
  {
    camera->Update(deltaTime);
  }
  for (const auto& gameObject : gameObjects)
  {
    // gameObject->rotation += glm::vec3(deltaTime / 2, deltaTime / 2, deltaTime / 2);
  }

  for (const auto& light : lights)
  {
    // circularly move the point light around the origin
    if (auto pointLight = std::dynamic_pointer_cast<PointLight>(light))
    {
      const float radius = 2.0f;
      const float speed = 0.5f; // radians per second
      const float angle = static_cast<float>(glfwGetTime()) * speed;
      pointLight->position =
        glm::vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
    }
  }

  for (const auto& volume : volumes)
  {
    if (volume)
    {
      if (std::shared_ptr<Shader> shader = volume->getShader())
      {
        (*shader)["time"] = static_cast<float>(glfwGetTime());
      }
    }
  }
}

/**
 * @brief Apply the scene's uniforms to the given shader
 * 
 * @param shader The shader to apply uniforms to
 */
void Scene::Apply(Shader& shader) const
{
  if (!shader.HasUniform("lightCount"))
  {
    return;
  }

  const int enabledCount = static_cast<int>(std::count_if(lights.begin(), lights.end(),
    [](const std::shared_ptr<Light>& light) { return light && light->GetEnabled(); }));
  const int lightCount = static_cast<int>(std::min(enabledCount, kMaxLights));
  shader.SetInt("lightCount", lightCount);
}

/**
 * @brief Scene renderer function
 * 
 */
void Scene::Render()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Count active lights
  const int enabledCount = static_cast<int>(std::count_if(lights.begin(), lights.end(),
    [](const std::shared_ptr<Light>& light) { return light && light->GetEnabled(); }));
  const int lightCount = static_cast<int>(std::min(enabledCount, kMaxLights));

  // Set up frame uniforms
  frameUniforms.ClearProviders();
  frameUniforms.AddProvider(*this);
  if (camera)
  {
    frameUniforms.AddProvider(*camera);
  }
  for (int i = 0; i < lightCount; ++i)
  {
    lights[static_cast<size_t>(i)]->SetUniformIndex(i);
    frameUniforms.AddProvider(*lights[static_cast<size_t>(i)]);
  }

  // Draw game objects
  for (const auto& gameObject : gameObjects)
  {
    gameObject->Draw(frameUniforms);
  }

  // Draw skybox
  if (skybox != nullptr)
  {
    if (camera)
    {
      skybox->Draw(*camera);
    }
  }

  // Draw volumes
  for (const auto& volume : volumes)
  {
    if (volume)
    {
      volume->Draw(frameUniforms);
    }
  }
}

/**
 * @brief Set the skybox for the scene
 * 
 * @param cubemap The cube map for the skybox
 */
void Scene::SetSkybox(std::shared_ptr<TextureCube> cubemap)
{
  if (cubemap != nullptr && cubemap->IsValid())
  {
    skybox = std::make_shared<Skybox>(std::move(cubemap));
    return;
  }

  skybox.reset();
}

/**
 * @brief Set the skybox for the scene
 * 
 * @param faces The faces of the skybox
 */
void Scene::SetSkybox(const SkyboxFaces& faces)
{
  SetSkybox(std::make_shared<TextureCube>(faces));
}

/**
 * @brief Clear the skybox for the scene
 * 
 */
void Scene::ClearSkybox()
{
  skybox.reset();
}

/**
 * @brief Add a light to the scene
 * 
 * @param light The light to add
 */
void Scene::AddLight(std::shared_ptr<Light> light)
{
  if (light)
  {
    lights.push_back(std::move(light));
  }
}

/**
 * @brief Clear all lights from the scene
 * 
 */
void Scene::ClearLights()
{
  lights.clear();
}

/**
 * @brief Add a game object to the scene
 * 
 * @param gameObject The game object to add
 */
void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject)
{
  if (gameObject)
  {
    gameObjects.push_back(std::move(gameObject));
  }
}

/**
 * @brief Clear all game objects from the scene
 * 
 */
void Scene::ClearGameObjects()
{
  gameObjects.clear();
}

/**
 * @brief Add a volume to the scene
 * 
 * @param volume The volume to add
 */
void Scene::AddVolume(std::shared_ptr<Volume> volume)
{
  if (volume)
  {
    volumes.push_back(std::move(volume));
  }
}

/**
 * @brief Clear all volumes from the scene
 * 
 */
void Scene::ClearVolumes()
{
  volumes.clear();
}

/**
 * @brief Get a reference to the scene's camera
 * 
 * @return Camera& 
 */
std::shared_ptr<Camera> Scene::GetCamera()
{
  return camera;
}

/**
 * @brief Set the aspect ratio for the scene's camera
 * 
 * @param aspect The aspect ratio
 */
void Scene::SetCameraAspect(float aspect)
{
  if (camera)
  {
    camera->SetAspect(aspect);
  }
}

/**
 * @brief Collect inspectable fields for the scene
 * 
 * @param out The vector to store the inspectable fields
 */
void Scene::CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix)
{
  const std::string prefix = groupPrefix.empty() ? "" : (groupPrefix + "/");

  for (auto& [shaderName, shader] : shaders)
  {
    if (!shader)
    {
      continue;
    }

    shader->CollectInspectableFields(out, prefix + "Shader/" + shaderName);
  }

  for (size_t i = 0; i < volumes.size(); ++i)
  {
    if (!volumes[i])
    {
      continue;
    }

    volumes[i]->CollectInspectableFields(out, prefix + "Volume/" + std::to_string(i));
  }

  for (size_t i = 0; i < gameObjects.size(); ++i)
  {
    if (!gameObjects[i])
    {
      continue;
    }

    gameObjects[i]->CollectInspectableFields(out, prefix + "GameObject/" + std::to_string(i));
  }

  for (size_t i = 0; i < lights.size(); ++i)
  {
    if (!lights[i])
    {
      continue;
    }

    lights[i]->CollectInspectableFields(out, prefix + "Light/" + std::to_string(i));
  }
}

/**
 * @brief Collect inspectable nodes for the scene
 * 
 * @param out The vector to store the inspectable nodes
 */
void Scene::CollectInspectableNodes(std::vector<InspectableNode>& out, const std::string& nodePrefix)
{
  const std::string prefix = nodePrefix.empty() ? "" : (nodePrefix + "/");

  // Collect shaders as nested nodes
  for (auto& [shaderName, shader] : shaders)
  {
    if (!shader)
    {
      continue;
    }

    InspectableNode node;
    node.nodeLabel = prefix + "Shader/" + shaderName;
    node.isField = false;
    node.nestedInspectable = std::static_pointer_cast<IInspectable>(shader);
    out.push_back(node);
  }

  // Collect volumes as nested nodes
  for (size_t i = 0; i < volumes.size(); ++i)
  {
    if (!volumes[i])
    {
      continue;
    }

    InspectableNode node;
    node.nodeLabel = prefix + "Volume/" + std::to_string(i);
    node.isField = false;
    node.nestedInspectable = std::static_pointer_cast<IInspectable>(volumes[i]);
    out.push_back(node);
  }

  // Collect game objects as nested nodes
  for (size_t i = 0; i < gameObjects.size(); ++i)
  {
    if (!gameObjects[i])
    {
      continue;
    }

    InspectableNode node;
    node.nodeLabel = prefix + "GameObject/" + std::to_string(i);
    node.isField = false;
    node.nestedInspectable = std::static_pointer_cast<IInspectable>(gameObjects[i]);
    out.push_back(node);
  }

  // Collect lights as nested nodes
  for (size_t i = 0; i < lights.size(); ++i)
  {
    if (!lights[i])
    {
      continue;
    }

    InspectableNode node;
    node.nodeLabel = prefix + "Light/" + std::to_string(i);
    node.isField = false;
    node.nestedInspectable = std::static_pointer_cast<IInspectable>(lights[i]);
    out.push_back(node);
  }
}

/**
 * @brief Destroy the Scene:: Scene object
 * 
 */
void Scene::Destroy()
{
}

/**
 * @brief Destroy the Scene:: Scene object
 * 
 */
Scene::~Scene()
{
}
