#include "Scene/Scene.h"
#include <algorithm>
#include <string>

#include "Mesh.h"
#include "Geometry/ModelLoader.h"
#include "Geometry/SphereGeometry.h"
#include "Texture/Skybox.h"
#include "Texture/Texture2D.h"
#include "Volume/FloatVolume.h"
#include "Volume/VolumeFileLoader.h"
#include <cmath>
#include <iostream>

#include <glm/gtc/constants.hpp>

namespace
{
  constexpr int kMaxLights = 16;
}

/**
 * @brief Construct a new Scene:: Scene object
 *        Set up default objects in the scene.
 *
 */
Scene::Scene()
    : camera(std::make_shared<PerspectiveCamera>(45.0f, 800.0f / 600.0f, 0.1f, 100.0f)), selfInspectProvider(static_cast<InspectProvider *>(this), [](InspectProvider *) {}) // non-owning alias; Scene owns itself
{
  // ------------- SHADERS (PBR) -------------
  std::shared_ptr<Shader> defaultShader = std::make_shared<Shader>(
      "default",
      "shaders/vertex.glsl",
      "shaders/pbr_fragment.glsl");

  std::shared_ptr<Shader> normalShader = std::make_shared<Shader>(
      "normalDebug",
      "shaders/vertex.glsl",
      "shaders/normal_fragment.glsl");

  // ------------- MATERIALS -------------
  std::shared_ptr<Material> boneMaterial = std::make_shared<Material>(defaultShader);
  boneMaterial->SetAlbedoTextureFromFile("assets/materials/bone/bone_albedo.png");
  boneMaterial->SetNormalTextureFromFile("assets/materials/bone/bone_normal-ogl.png");
  boneMaterial->SetRoughnessTextureFromFile("assets/materials/bone/bone_roughness.png");
  boneMaterial->SetMetallicTextureFromFile("assets/materials/bone/bone_metallic.png");

  std::shared_ptr<Material> donutMaterial = std::make_shared<Material>(defaultShader);
  donutMaterial->SetAlbedoTextureFromFile("assets/materials/donut/Poliigon_FoodPastryDonut_10737_BaseColor.jpg");
  donutMaterial->SetNormalTextureFromFile("assets/materials/donut/Poliigon_FoodPastryDonut_10737_Normal.png");
  donutMaterial->SetRoughnessTextureFromFile("assets/materials/donut/Poliigon_FoodPastryDonut_10737_Roughness.jpg");
  donutMaterial->SetMetallicTextureFromFile("assets/materials/donut/Poliigon_FoodPastryDonut_10737_Metallic.jpg");

  std::shared_ptr<Material> dragonMaterial = std::make_shared<Material>(defaultShader);
  dragonMaterial->SetAlbedoTextureFromFile("assets/materials/dragon/DefaultMaterial_albedo.jpg");
  dragonMaterial->SetNormalTextureFromFile("assets/materials/dragon/DefaultMaterial_normal.png");
  dragonMaterial->SetRoughnessTextureFromFile("assets/materials/dragon/DefaultMaterial_roughness.jpg");
  dragonMaterial->SetMetallicTextureFromFile("assets/materials/dragon/DefaultMaterial_metallic.jpg");

  std::shared_ptr<Material> assasinMaterial = std::make_shared<Material>(defaultShader);
  assasinMaterial->SetAlbedoTextureFromFile("assets/materials/assasin/Royalshadowassassin3dmodel_basecolor.jpeg");

  std::shared_ptr<Material> normalDebugMaterial = std::make_shared<Material>(normalShader);
  // ------------- GAME OBJECTS -------------
  auto sphere2 = std::make_shared<GameObject>(
      std::make_shared<SphereGeometry>(0.2f, 64, 32),
      donutMaterial,
      "sphere2");
  sphere2->SetPosition(glm::vec3(0.5f, 0.0f, 0.0f));

  auto floor = std::make_shared<GameObject>(
      std::make_shared<QuadGeometry>(),
      boneMaterial,
      "floor");
  floor->SetPosition(glm::vec3(0.0f, -0.2f, 0.0f));
  floor->SetRotation(glm::vec3(-glm::half_pi<float>(), 0.0f, 0.0f));
  floor->SetScale(glm::vec3(10.0f));

  auto dragon = ModelLoader::LoadGameObject("assets/models/dragon.dae", defaultShader);
  dragon->SetMaterial(dragonMaterial);
  dragon->SetPosition(glm::vec3(-0.5f, -0.2f, 0.0f));
  dragon->SetRotation(glm::vec3(0.0f, glm::pi<float>() / 3.0f, 0.0f));
  dragon->SetScale(glm::vec3(0.01f));

  auto assasin = ModelLoader::LoadGameObject("assets/models/assasin.fbx", defaultShader);
  assasin->SetMaterial(assasinMaterial);
  assasin->SetPosition(glm::vec3(0.0f, -0.2f, 0.0f));
  assasin->SetRotation(glm::vec3(0.0f, glm::pi<float>() / 2.0f, 0.0f));
  assasin->SetScale(glm::vec3(0.01f));

  auto bunny = ModelLoader::LoadGameObject("assets/models/stanford_bunny.obj", normalShader);
  bunny->SetMaterial(normalDebugMaterial);
  bunny->SetPosition(glm::vec3(0.0f, -0.2f, 0.0f));
  bunny->SetRotation(glm::vec3(0.0f, glm::pi<float>() / 2.0f, 0.0f));
  bunny->SetScale(glm::vec3(0.01f));

  AddGameObject(bunny);
  AddGameObject(assasin);
  AddGameObject(sphere2);
  AddGameObject(dragon);
  AddGameObject(floor);
  // ------------- VOLUME -------------

  // ------------- LIGHTS -------------
  std::shared_ptr<DirectionalLight> directionalLight = std::make_shared<DirectionalLight>(
      "dirLight",
      glm::vec3(-1.0f, 0.0f, 0.0f),
      glm::vec3(0.5f, 0.5f, 0.5f),
      glm::vec3(1.0f, 1.0f, 1.0f),
      glm::vec3(-1.0f, -1.0f, -1.0f));
  AddLight(directionalLight);

  std::shared_ptr<DirectionalLight> directionalLight2 = std::make_shared<DirectionalLight>(
      "dirLight2",
      glm::vec3(1.0f, 0.0f, 0.0f),
      glm::vec3(0.5f, 0.5f, 0.5f),
      glm::vec3(1.0f, 1.0f, 1.0f),
      glm::vec3(-1.0f, -1.0f, -1.0f));
  AddLight(directionalLight2);

  // SetSkybox(SkyboxFaces{
  //   "assets/textures/skybox/pz.png",
  //   "assets/textures/skybox/nz.png",
  //   "assets/textures/skybox/py.png",
  //   "assets/textures/skybox/ny.png",
  //   "assets/textures/skybox/px.png",
  //   "assets/textures/skybox/nx.png"
  // });
}

/**
 * @brief Initialize the scene,
 *        enable depth testing, set clear color, set depth function,
 *        enable seamless cubemap sampling
 *
 */
void Scene::Init()
{
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
  for (const auto &updateable : updateables)
  {
    if (updateable)
    {
      updateable->Update(deltaTime);
    }
  }

  for (const auto &light : lights)
  {
    if (light)
    {
      light->Update(deltaTime);
    }
  }

  // Clear per-frame input impulses after the update pass consumed them.
  inputState.ResetFrameTransientState();
}

/**
 * @brief Apply the scene's uniforms to the given shader.
 *
 * @param shader The shader to apply uniforms to
 */
void Scene::Apply(Shader &shader) const
{
  const int enabledCount = static_cast<int>(std::count_if(lights.begin(), lights.end(),
                                                          [](const auto &lightEntry)
                                                          {
                                                            const std::shared_ptr<Light> &light = lightEntry;
                                                            return light && light->GetEnabled();
                                                          }));
  const int lightCount = static_cast<int>(std::min(enabledCount, kMaxLights));
  shader.SetInt("lightCount", lightCount);
}

/**
 * @brief Clear all game objects from the scene
 *
 */
void Scene::ClearGameObjects()
{
  drawables.erase(std::remove_if(drawables.begin(), drawables.end(),
                                 [](const std::shared_ptr<IDrawable> &drawable)
                                 {
                                   return std::dynamic_pointer_cast<GameObject>(drawable) != nullptr;
                                 }),
                  drawables.end());

  updateables.erase(std::remove_if(updateables.begin(), updateables.end(),
                                   [](const std::shared_ptr<IUpdateable> &updateable)
                                   {
                                     return std::dynamic_pointer_cast<GameObject>(updateable) != nullptr;
                                   }),
                    updateables.end());
}

/**
 * @brief Clear all volumes from the scene
 *
 */
void Scene::ClearVolumes()
{
  drawables.erase(std::remove_if(drawables.begin(), drawables.end(),
                                 [](const std::shared_ptr<IDrawable> &drawable)
                                 {
                                   return std::dynamic_pointer_cast<Volume>(drawable) != nullptr;
                                 }),
                  drawables.end());

  updateables.erase(std::remove_if(updateables.begin(), updateables.end(),
                                   [](const std::shared_ptr<IUpdateable> &updateable)
                                   {
                                     return std::dynamic_pointer_cast<Volume>(updateable) != nullptr;
                                   }),
                    updateables.end());
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
    // Remove any existing skybox drawable first
    if (skybox)
    {
      drawables.erase(std::remove_if(drawables.begin(), drawables.end(),
                                     [this](const std::shared_ptr<IDrawable> &d)
                                     {
                                       return std::dynamic_pointer_cast<Skybox>(d) != nullptr;
                                     }),
                      drawables.end());
    }

    skybox = std::make_shared<Skybox>(std::move(cubemap));
    // Add skybox to drawables so it participates in proxy building
    AddDrawable(skybox);
    return;
  }

  skybox.reset();
}

/**
 * @brief Set the skybox for the scene
 *
 * @param faces The faces of the skybox
 */
void Scene::SetSkybox(const SkyboxFaces &faces)
{
  SetSkybox(std::make_shared<TextureCube>(faces));
}

/**
 * @brief Clear the skybox for the scene
 *
 */
void Scene::ClearSkybox()
{
  // Remove any skybox drawable from drawables list
  drawables.erase(std::remove_if(drawables.begin(), drawables.end(),
                                 [](const std::shared_ptr<IDrawable> &d)
                                 {
                                   return std::dynamic_pointer_cast<Skybox>(d) != nullptr;
                                 }),
                  drawables.end());

  skybox.reset();
}

/**
 * @brief Register a shader with the scene for hot reload tracking
 *
 * @param name The identifier name for this shader
 * @param shader The shader to track
 */
void Scene::RegisterShader(const std::string &name, std::shared_ptr<Shader> shader)
{
  if (!shader)
  {
    return;
  }

  for (auto &registeredShader : shaders)
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
  for (auto &shader : shaders)
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

/**
 * @brief Builds a list of RenderProxies that represent every object that should be rendered on the screen.
 *        The Renderer will comsume this list.
 *
 * @return std::vector<RenderProxy>
 */
std::vector<RenderProxy> Scene::GetRenderProxies() const
{
  std::vector<RenderProxy> proxies;

  // Set up basic uniform provider list
  // consising of the scene itself, the camera, and all enabled lights.
  CompositeUniformProvider sceneUniforms;
  sceneUniforms.AddProvider(*this);
  if (camera)
  {
    sceneUniforms.AddProvider(*camera);
  }

  int lightUniformIndex = 0;
  for (const auto &light : lights)
  {
    if (!light || !light->GetEnabled())
    {
      continue;
    }

    light->SetUniformIndex(lightUniformIndex++);
    sceneUniforms.AddProvider(*light);
  }

  // Each drawable builds a render proxy
  for (const auto &drawable : drawables)
  {
    // Skip null drawables
    if (!drawable)
    {
      continue;
    }

    RenderProxy proxy;
    proxy.frameUniforms = sceneUniforms;
    proxy.visible = true;

    drawable->BuildRenderProxy(proxy);

    // If it actually should be drawn then add it to the list for the renderer
    if (proxy.visible)
    {
      proxies.push_back(proxy);
    }
  }

  return proxies;
}

SceneSnapshot Scene::CreateSnapshot() const
{
  return SceneSnapshot(GetRenderProxies());
}

/**
 * @brief Rebuild the inspect providers for the scene, including the scene itself,
 *  the camera, and all lights and drawables that implement InspectProvider
 *
 */
void Scene::RebuildInspectProviders()
{
  inspectProviderEntries.clear();
  ++providerGeneration;

  auto addProvider = [this](const std::shared_ptr<InspectProvider> &provider, const std::string &idPrefix, int index)
  {
    if (!provider)
    {
      return;
    }
    inspectProviderEntries.push_back({provider, idPrefix + "#" + std::to_string(index)});
  };

  addProvider(selfInspectProvider, "scene", 0);
  addProvider(std::dynamic_pointer_cast<InspectProvider>(camera), "camera", 0);

  int lightIndex = 0;
  for (const auto &light : lights)
  {
    addProvider(std::dynamic_pointer_cast<InspectProvider>(light), "light", lightIndex++);
  }

  int drawableIndex = 0;
  for (const auto &drawable : drawables)
  {
    if (auto provider = std::dynamic_pointer_cast<InspectProvider>(drawable))
    {
      addProvider(provider, "drawable", drawableIndex++);
    }
  }
}

/**
 * @brief Get the inspect fields for the scene, clear color in this case
 *
 * @return engine inspection fields
 */
std::vector<InspectFieldPtr> Scene::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields;
  return fields;
}
