#pragma once
#include <unordered_map>
#include <vector>
#include <map>
#include <string>

#include "Geometry/Triangle.h"
#include "Geometry/QuadGeometry.h"
#include "Shader.h"
#include "Camera/Camera.h"
#include "Camera/PerspectiveCamera.h"
#include "Uniform/CompositeUniformProvider.h"
#include "Light/PointLight.h"
#include "Light/DirectionalLight.h"
#include "Light/Light.h"
#include "GameObject.h"
#include "Material.h"
#include "Texture/TextureCube.h"
#include "Uniform/UniformProvider.h"
#include "Volume/Volume.h"
#include "Input/InputState.h"
#include "IDrawable.h"
#include "IUpdateable.h"
#include "RenderCore/SceneSnapshot.h"
#include "Inspection/InspectField.h"
#include <memory>
#include "Renderer/RenderProxy.h"

class Shader;
class Triangle;
class Skybox;
class TextureCube;
class Volume;

class Scene : public UniformProvider, InspectProvider
{
public:
  Scene();
  ~Scene();
  
  void Init();
  virtual void Update(float deltaTime);
  void Render();
  void Destroy();
  void Apply(Shader& shader) const override;

  // Skybox management
  void SetSkybox(std::shared_ptr<TextureCube> cubemap);
  void SetSkybox(const SkyboxFaces& faces);
  void ClearSkybox();

  // Camera management
  std::shared_ptr<Camera> GetCamera() { return camera; }
  void SetCameraAspect(float aspect);

  // Render proxy gathering
  std::vector<RenderProxy> GetRenderProxies() const;
  SceneSnapshot CreateSnapshot() const;

  // Input state management
  const InputState& GetInputState() const { return inputState; }
  void SetInputState(const InputState& inputStateValue) { inputState = inputStateValue; }

  // Scene content management
  void AddLight(std::shared_ptr<Light> light) { lights.push_back(light); }
  void ClearLights()  { lights.clear(); }

  void AddDrawable(std::shared_ptr<IDrawable> drawable) { drawables.push_back(drawable); }
  void ClearDrawables() { drawables.clear(); }

  void AddUpdateable(std::shared_ptr<IUpdateable> updateable) { updateables.push_back(updateable); }
  void ClearUpdateables() { updateables.clear(); }

  void AddGameObject(std::shared_ptr<GameObject> gameObject)
  {
    AddDrawable(gameObject);
    AddUpdateable(gameObject);
  }
  void ClearGameObjects();

  void AddVolume(std::shared_ptr<Volume> volume)
  {
    AddDrawable(volume);
    AddUpdateable(volume);
  }
  void ClearVolumes();

  void RebuildInspectProviders();

  // Shader management
  void RegisterShader(const std::string& name, std::shared_ptr<Shader> shader);

  // Hot reload: recheck all shader files and reload if changed
  void ReloadShadersIfChanged();

  // Inspection provider discovery: weak_ptr-backed so callers never hold a dangling raw pointer
  struct InspectProviderEntry
  {
    std::weak_ptr<InspectProvider> provider;
    std::string id; // stable across rebuilds as long as the same object persists
  };
  std::vector<InspectProviderEntry> GetInspectProviderEntries() const { return inspectProviderEntries; }

  // Increments every time RebuildInspectProviders() runs; lets consumers detect structural changes cheaply.
  int GetProviderGeneration() const { return providerGeneration; }

  // Inspect provider implementation
  std::string GetInspectDisplayName() const override { return "Scene"; }
  std::vector<InspectFieldPtr> GetInspectFields() override;
  bool HasVisibility() const { return false; }
  bool IsVisible() const { return true; }
  std::optional<float> CastRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const
  {
    return std::nullopt; // Scene itself is not selectable by ray
  }


private:
  std::shared_ptr<Camera> camera;
  CompositeUniformProvider frameUniforms;

  std::shared_ptr<Skybox> skybox;
  std::vector<std::shared_ptr<Light>> lights;
  std::vector<std::shared_ptr<IDrawable>> drawables;
  std::vector<std::shared_ptr<IUpdateable>> updateables;
  std::vector<std::shared_ptr<Shader>> shaders;
  InputState inputState;

  std::shared_ptr<InspectProvider> selfInspectProvider; // non-owning alias of `this`; Scene's lifetime always outlives its use
  std::vector<InspectProviderEntry> inspectProviderEntries;
  int providerGeneration = 0;
};
