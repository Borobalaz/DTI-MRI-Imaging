#include "Scene/Scene.h"
#include <algorithm>
#include <string>

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

#include "ui/widgets/inspect_fields/InspectColorFieldWidget.h"

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
    camera(std::make_shared<PerspectiveCamera>(45.0f, 800.0f / 600.0f, 0.1f, 100.0f))
{
  AddInspectProvider(this);
  AddInspectProvider(camera);

  // ------------- SHADERS -------------

  // ------------- MATERIALS -------------

  // ------------- GAME OBJECTS -------------
  
  // ------------- VOLUME -------------

  // ------------- LIGHTS -------------
  std::shared_ptr<PointLight> light0 = std::make_shared<PointLight>(PointLight(
    "point_0",
    glm::vec3(0.0f, 0.0f, 2.0f),
    glm::vec3(1.0f, 0.08f, 0.08f),
    glm::vec3(0.9f, 0.9f, 0.9f),
    glm::vec3(1.0f, 1.0f, 1.0f),
    1.0f,
    0.09f,
    0.032f
  ));
  AddLight(light0);
  AddInspectProvider(light0);

  std::shared_ptr<DirectionalLight> light1 = std::make_shared<DirectionalLight>(DirectionalLight(
    "directional_0",
    glm::vec3(-0.2f, -1.0f, -0.3f),
    glm::vec3(0.05f, 0.05f, 0.05f),
    glm::vec3(0.45f, 0.45f, 0.45f),
    glm::vec3(0.35f, 0.35f, 0.35f)
  ));
  AddLight(light1);
  AddInspectProvider(light1);
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

  // Clear per-frame input impulses after the update pass consumed them.
  inputState.ResetFrameTransientState();
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
    [](const auto& lightEntry)
    {
      const std::shared_ptr<Light>& light = lightEntry;
      return light && light->GetEnabled();
    }));
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
    [](const auto& lightEntry)
    {
      const std::shared_ptr<Light>& light = lightEntry;
      return light && light->GetEnabled();
    }));
  const int lightCount = static_cast<int>(std::min(enabledCount, kMaxLights));

  std::vector<std::shared_ptr<Light>> enabledLights;
  enabledLights.reserve(static_cast<size_t>(lightCount));
  for (const auto& light : lights)
  {
    if (!light || !light->GetEnabled())
    {
      continue;
    }

    enabledLights.push_back(light);
    if (static_cast<int>(enabledLights.size()) >= lightCount)
    {
      break;
    }
  }

  // Set up frame uniforms
  frameUniforms.ClearProviders();
  frameUniforms.AddProvider(*this);
  if (camera)
  {
    frameUniforms.AddProvider(*camera);
  }
  for (int i = 0; i < static_cast<int>(enabledLights.size()); ++i)
  {
    enabledLights[static_cast<size_t>(i)]->SetUniformIndex(i);
    frameUniforms.AddProvider(*enabledLights[static_cast<size_t>(i)]);
  }

  // Draw game objects
  for (const auto& gameObject : gameObjects)
  {
    if (gameObject)
    {
      gameObject->Draw(frameUniforms);
    }
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
 * @brief Register a shader with the scene for hot reload tracking
 * 
 * @param name The identifier name for this shader
 * @param shader The shader to track
 */
void Scene::RegisterShader(const std::string& name, std::shared_ptr<Shader> shader)
{
  if (!shader)
  {
    return;
  }

  for (auto& registeredShader : shaders)
  {
    if (registeredShader && registeredShader->GetId() == name)
    {
      registeredShader = std::move(shader);
      return;
    }
  }

  shaders.emplace_back(std::move(shader));
}

/**
 * @brief Hot reload: check all shader files for modifications and reload if changed
 * 
 */
void Scene::ReloadShadersIfChanged()
{
  for (auto& shader : shaders)
  {
    if (shader && shader->ReloadIfChanged())
    {
      // Shader was reloaded successfully
    }
  }
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

std::vector<std::shared_ptr<IInspectWidget>> Scene::GetInspectFields()
{
  std::vector<std::shared_ptr<IInspectWidget>> fields;

  auto clearColorField = std::make_shared<InspectColorFieldWidget>("clearColor", "Clear Color", "Color");
  clearColorField->SetValue(QVariantList{clearColor[0], clearColor[1], clearColor[2]});
  clearColorField->valueChangedCallback = [this](const QVariant &value)
  {
    const QVariantList list = value.toList();
    if (list.size() >= 3)
    {
      clearColor[0] = static_cast<float>(list[0].toDouble());
      clearColor[1] = static_cast<float>(list[1].toDouble());
      clearColor[2] = static_cast<float>(list[2].toDouble());
    }
  };
  fields.push_back(clearColorField);

  return fields;
}
