#include "Scene/Scene.h"
#include <algorithm>

#include "Mesh.h"
#include "Geometry/ModelLoader.h"
#include "Texture/Skybox.h"
#include "Texture/Texture2D.h"
#include "Volume/FloatVolume.h"
#include "Volume/VolumeFileLoader.h"

#include <chrono>
#include <cmath>
#include <iostream>

#include <glm/gtc/constants.hpp>

namespace
{
  constexpr int kMaxLights = 16;

  float SceneElapsedSeconds()
  {
    static const auto startTime = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = now - startTime;
    return std::chrono::duration<float>(elapsed).count();
  }

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
  mandelbulbVolume->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
  mandelbulbVolume->SetScale(glm::vec3(1.5f, 1.5f, 1.5f));

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
      const float angle = SceneElapsedSeconds() * speed;
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
        (*shader)["time"] = SceneElapsedSeconds();
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
  // Qt Quick can reset GL state between frames, so enforce depth state before drawing.
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

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
 * @brief Register a shader with the scene for hot reload tracking
 * 
 * @param name The identifier name for this shader
 * @param shader The shader to track
 */
void Scene::RegisterShader(const std::string& name, std::shared_ptr<Shader> shader)
{
  if (shader)
  {
    shaders[name] = shader;
  }
}

/**
 * @brief Hot reload: check all shader files for modifications and reload if changed
 * 
 */
void Scene::ReloadShadersIfChanged()
{
  for (auto& [name, shader] : shaders)
  {
    if (shader && shader->ReloadIfChanged())
    {
      // Shader was reloaded successfully
    }
  }
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
