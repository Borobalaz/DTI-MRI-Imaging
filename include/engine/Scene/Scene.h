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
#include "ui/widgets/inspect_fields/InspectProvider.h"
#include <memory>

class Shader;
class Triangle;
class Skybox;
class TextureCube;
class Volume;

class Scene : public UniformProvider
{
public:
  Scene();
  ~Scene();
  
  void Init();
  void Update(float deltaTime);
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

  // Input state management
  const InputState& GetInputState() const { return inputState; }
  void SetInputState(const InputState& inputStateValue) { inputState = inputStateValue; }

  // Scene content management
  void AddLight(std::shared_ptr<Light> light) { lights.push_back(light); }
  void ClearLights()  { lights.clear(); }

  void AddGameObject(std::shared_ptr<GameObject> gameObject) { gameObjects.push_back(gameObject); }
  void ClearGameObjects() { gameObjects.clear(); }

  void AddVolume(std::shared_ptr<Volume> volume) { volumes.push_back(volume); }
  void ClearVolumes() { volumes.clear(); }

  void AddInspectProvider(std::shared_ptr<InspectProvider> provider) { inspectProviders.push_back(provider.get()); }

  // Shader management
  void RegisterShader(const std::string& name, std::shared_ptr<Shader> shader);

  // Hot reload: recheck all shader files and reload if changed
  void ReloadShadersIfChanged();

  // Inspection provider discovery
  std::vector<InspectProvider*> GetInspectProviders() const { return inspectProviders; }
  std::vector<std::string> GetInspectProviderNames() const;

private:
  void RebuildInspectProviders();

  float clearColor[4];

  std::shared_ptr<Camera> camera;
  CompositeUniformProvider frameUniforms;

  std::shared_ptr<Skybox> skybox;

  std::vector<std::shared_ptr<Light>> lights;
  std::vector<std::shared_ptr<Volume>> volumes;
  std::vector<std::shared_ptr<GameObject>> gameObjects;
  std::vector<std::shared_ptr<Shader>> shaders;
  InputState inputState;

  std::vector<InspectProvider*> inspectProviders;
};

