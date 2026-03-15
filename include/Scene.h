#pragma once
#include <unordered_map>
#include <vector>

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
#include "Material.h"
#include "TextureCube.h"
#include "VolumeObject.h"
#include "VolumeRenderSettings.h"
#include <memory>

class Shader;
class Triangle;
class Skybox;
class TextureCube;
class VolumeObject;

class Scene
{
public:
  Scene();
  ~Scene();
  
  void Init();
  void Update(float deltaTime);
  void Render();
  void Destroy();

  void SetSkybox(std::shared_ptr<TextureCube> cubemap);
  void SetSkybox(const SkyboxFaces& faces);
  void ClearSkybox();

  void ClearVolumes();

  Camera& GetCamera();

private:
  float clearColor[4];

  PerspectiveCamera camera;
  CompositeUniformProvider frameUniforms;
  std::vector<std::shared_ptr<Light>> lights;
  std::shared_ptr<Skybox> skybox;
  std::vector<std::shared_ptr<VolumeObject>> volumes;
  
  std::vector<std::shared_ptr<GameObject>> gameObjects;
  std::shared_ptr<Shader> basicShader;
};
