#include "Scene/Scene.h"
#include <algorithm>
#include <string>

#include "Camera/InspectionCameraMovement.h"
#include "Mesh.h"
#include "Geometry/ModelLoader.h"
#include "Geometry/SphereGeometry.h"
#include "Texture/Skybox.h"
#include "Render/Texture/Texture2D.h"
#include "Volume/FloatVolume.h"
#include "Volume/VolumeFileLoader.h"
#include <cmath>
#include <iostream>

#include "Log/EngineLog.h"

#include <glm/gtc/constants.hpp>

namespace
{
  constexpr int kMaxLights = 16;

  // Removes every element whose runtime type is TargetType from a vector of
  // base-class shared_ptrs. Shared by Scene::ClearGameObjects/ClearVolumes/
  // SetSkybox/ClearSkybox, which all need to prune drawables/updateables by
  // concrete type.
  template <typename TargetType, typename ElementType>
  void RemoveElementsOfType(std::vector<std::shared_ptr<ElementType>> &elements)
  {
    elements.erase(std::remove_if(elements.begin(), elements.end(),
                                  [](const std::shared_ptr<ElementType> &element)
                                  {
                                    return std::dynamic_pointer_cast<TargetType>(element) != nullptr;
                                  }),
                  elements.end());
  }
}

/**
 * @brief Construct a new Scene:: Scene object
 *        Set up default objects in the scene.
 *
 */
Scene::Scene()
    : camera(std::make_shared<PerspectiveCamera>(45.0f, 800.0f / 600.0f, 0.1f, 100.0f)), selfInspectProvider(static_cast<InspectProvider *>(this), [](InspectProvider *) {}) // non-owning alias; Scene owns itself
{
  // ------------- CAMERA MOVEMENT -------------
  // Scene owns the camera, so it owns wiring up its default interactive
  // movement strategy too; it reads from the scene's own InputState, which
  // Engine refreshes every frame via SetInputState().
  auto cameraMovement = std::make_unique<InspectionCameraMovement>();
  cameraMovement->SetInputState(&inputState);
  camera->SetMoveComponent(std::move(cameraMovement));

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

  std::shared_ptr<Material> normalDebugMaterial = std::make_shared<Material>(normalShader);
  // ------------- GAME OBJECTS -------------

  // ------------- VOLUME -------------

  // ------------- LIGHTS -------------
  std::shared_ptr<DirectionalLight> directionalLight = std::make_shared<DirectionalLight>(
      "dirLight",
      glm::vec3(-1.0f, 0.0f, 0.0f),
      glm::vec3(0.5f, 0.5f, 0.5f),
      glm::vec3(1.0f, 1.0f, 1.0f),
      glm::vec3(-1.0f, -1.0f, -1.0f));
  AddLight(directionalLight);

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
void Scene::Apply(IUniformSink &shader) const
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
  RemoveElementsOfType<GameObject>(drawables);
  RemoveElementsOfType<GameObject>(updateables);
}

/**
 * @brief Clear all volumes from the scene
 *
 */
void Scene::ClearVolumes()
{
  RemoveElementsOfType<Volume>(drawables);
  RemoveElementsOfType<Volume>(updateables);
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
      RemoveElementsOfType<Skybox>(drawables);
    }

    skybox = std::make_shared<Skybox>(std::move(cubemap));
    // Add skybox to drawables so it participates in proxy building
    AddDrawable(skybox);
    return;
  }

  EngineLog::Warning("SetSkybox called with a null or invalid cubemap; resetting to no skybox.", "Scene");
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
  RemoveElementsOfType<Skybox>(drawables);
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
    EngineLog::Warning("RegisterShader called with a null shader for '" + name + "'.", "Scene");
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
      EngineLog::Message("Shader reloaded: " + shader->GetId(), "Scene");
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

// Currently a no-op: all scene resources are owned by shared_ptrs and clean
// themselves up via normal destruction. Kept as an explicit lifecycle hook
// for callers that want a deterministic teardown point before destruction.
void Scene::Destroy()
{
}

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
