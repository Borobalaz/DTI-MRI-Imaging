#include "Scene.h"
#include <algorithm>

#include "Mesh.h"
#include "ModelLoader.h"
#include "Skybox.h"
#include "Texture2D.h"
#include "VolumeFileLoader.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <filesystem>

namespace
{
  constexpr int kMaxLights = 16;

  void EnsureDemoVolumeFile(const std::string& filePath)
  {
    if (std::filesystem::exists(filePath))
    {
      return;
    }

    constexpr int size = 48;
    VolumeData<float> volume(size, size, size);

    for (int z = 0; z < size; ++z)
    {
      for (int y = 0; y < size; ++y)
      {
        for (int x = 0; x < size; ++x)
        {
          const glm::vec3 position = (glm::vec3(x, y, z) / glm::vec3(size - 1) - 0.5f) * 2.0f;
          const float radius = glm::length(position);
          volume.At(x, y, z) = glm::max(0.0f, 1.0f - radius);
        }
      }
    }

    VolumeFileLoader::Save(filePath, volume);
  }
}

/**
 * @brief Construct a new Scene:: Scene object
 * 
 */
Scene::Scene()
  : clearColor{0.8f, 0.3f, 0.3f, 1.0f},
    camera(45.0f, 800.0f / 600.0f, 0.1f, 100.0f)
{

  // SHADERS
  basicShader = std::make_shared<Shader>(
    "shaders/vertex.glsl",
    "shaders/fragment.glsl"
  );

  // MATERIALS
  std::shared_ptr<Material> triangleMaterial = std::make_shared<Material>(basicShader);
  triangleMaterial->SetTexture(std::make_shared<Texture2D>("assets/textures/balazslogo.png"));

  // GAME OBJECTS
  const std::shared_ptr<GameObject> importedModel =
    ModelLoader::LoadGameObject("assets/models/assasin.fbx", basicShader);
  if (importedModel)
  {
    importedModel->position = glm::vec3(0.0f, 0.0f, 0.0f);
    importedModel->scale = glm::vec3(1.0f, 1.0f, 1.0f);
    importedModel->rotation = glm::vec3(-glm::half_pi<float>(), 0.0f, -glm::half_pi<float>());
    gameObjects.push_back(importedModel);
  }
  else
  {
    //std::cout << "Failed to load model\n";
  }

  // LIGHTS
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

  EnsureDemoVolumeFile("assets/volumes/demo_scalar.vxa");
  VolumeRenderSettings demoSettings;
  demoSettings.stepSize = 0.02f;
  demoSettings.opacityScale = 0.18f;
  demoSettings.intensityScale = 1.8f;
  demoSettings.threshold = 0.15f;
  demoSettings.maxSteps = 192;

  const std::shared_ptr<VolumeObject> demoVolume =
    VolumeObject::CreateFromFile("assets/volumes/demo_scalar.vxa", demoSettings);
  if (demoVolume)
  {
    demoVolume->position = glm::vec3(1.75f, 0.0f, 0.0f);
    demoVolume->scale = glm::vec3(1.4f, 1.4f, 1.4f);
    volumes.push_back(demoVolume);
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

void Scene::ClearVolumes()
{
  volumes.clear();
}

/**
 * @brief Get a reference to the scene's camera
 * 
 * @return Camera& 
 */
Camera& Scene::GetCamera()
{
  return camera;
}

/**
 * @brief Initialize the scene
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
  camera.Update(deltaTime);
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
}

/**
 * @brief Scene renderer function
 * 
 */
void Scene::Render()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  Shader& shader = *basicShader;
  shader.Use();

  const int lightCount = static_cast<int>(std::min(lights.size(), static_cast<size_t>(kMaxLights)));
  shader.SetInt("lightCount", lightCount);

  frameUniforms.ClearProviders();
  frameUniforms.AddProvider(camera);
  for (int i = 0; i < lightCount; ++i)
  {
    lights[static_cast<size_t>(i)]->SetUniformIndex(i);
    frameUniforms.AddProvider(*lights[static_cast<size_t>(i)]);
  }

  for (const auto& gameObject : gameObjects)
  {
    gameObject->Draw(frameUniforms);
  }

  if (skybox != nullptr)
  {
    skybox->Draw(camera);
  }

  CompositeUniformProvider volumeFrameUniforms;
  volumeFrameUniforms.AddProvider(camera);
  for (const auto& volume : volumes)
  {
    if (volume)
    {
      volume->Draw(volumeFrameUniforms);
    }
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
