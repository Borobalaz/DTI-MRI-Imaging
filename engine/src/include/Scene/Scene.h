#pragma once
#include <unordered_map>
#include <vector>
#include <map>
#include <string>

#include "Geometry/Triangle.h"
#include "Geometry/QuadGeometry.h"
#include "Render/Shader.h"
#include "Camera/Camera.h"
#include "Camera/PerspectiveCamera.h"
#include "Render/Uniform/CompositeUniformProvider.h"
#include "Light/PointLight.h"
#include "Light/DirectionalLight.h"
#include "Light/Light.h"
#include "GameObject.h"
#include "Glyphs/SHGlyphField.h"
#include "Render/Material.h"
#include "Render/Texture/TextureCube.h"
#include "Render/Uniform/UniformProvider.h"
#include "Volume/Volume.h"
#include "Input/InputState.h"
#include "IDrawable.h"
#include "IUpdateable.h"
#include "RenderCore/SceneSnapshot.h"
#include "inspection/InspectField.h"
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

  virtual void Update(float deltaTime);
  void Render();
  void Destroy();
  void Apply(IUniformSink& shader) const override;

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
  void ClearLights();

  void AddDrawable(std::shared_ptr<IDrawable> drawable) { drawables.push_back(drawable); }
  void ClearDrawables();

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

  void AddGlyphField(std::shared_ptr<SHGlyphField> glyphField)
  {
    AddDrawable(glyphField);
    AddUpdateable(glyphField);
  }
  void ClearGlyphFields();

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

  // Increments every time RebuildInspectProviders() runs and every time a method removes objects
  // that may be inspect providers (Clear*, SetSkybox replacing a skybox). Lets consumers detect
  // structural changes cheaply, and lets LockedInspectionService retire fields whose provider
  // may be gone. Any new method that removes providers must bump it too.
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
