#pragma once
#include <unordered_map>
#include <vector>
#include <map>

#include "Triangle.h"
#include "QuadGeometry.h"
#include "Shader.h"
#include "Camera.h"
#include "PerspectiveCamera.h"
#include "CompositeUniformProvider.h"
#include "PointLight.h"
#include "DirectionalLight.h"
#include "Light.h"
#include "GameObject.h"
#include "Gui/Inspectable.h"
#include "Material.h"
#include "TextureCube.h"
#include "UniformProvider.h"
#include "Volume.h"
#include <memory>

class Shader;
class Triangle;
class Skybox;
class TextureCube;
class Volume;

class Scene : public UniformProvider, public IInspectable
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
  std::shared_ptr<Camera> GetCamera();
  void SetCameraAspect(float aspect);

  // IInspectable implementation
  void CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix = "") override;
  void CollectInspectableNodes(std::vector<InspectableNode>& out, const std::string& nodePrefix = "") override;

  // Scene content management
  void AddLight(std::shared_ptr<Light> light);
  void ClearLights();

  void AddGameObject(std::shared_ptr<GameObject> gameObject);
  void ClearGameObjects();

  void AddVolume(std::shared_ptr<Volume> volume);
  void ClearVolumes();

  // Shader management
  void RegisterShader(const std::string& name, std::shared_ptr<Shader> shader);

  // Hot reload: recheck all shader files and reload if changed
  void ReloadShadersIfChanged();

private:
  float clearColor[4];

  std::shared_ptr<Camera> camera;
  CompositeUniformProvider frameUniforms;

  std::shared_ptr<Skybox> skybox;

  std::vector<std::shared_ptr<Light>> lights;
  std::vector<std::shared_ptr<Volume>> volumes;
  std::vector<std::shared_ptr<GameObject>> gameObjects;
  std::map<std::string, std::shared_ptr<Shader>> shaders;

  glm::vec3 matrixTestUniformValue;
};
