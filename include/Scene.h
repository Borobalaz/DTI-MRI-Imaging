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
#include "Volume.h"
#include <memory>

class Shader;
class Triangle;
class Skybox;
class TextureCube;
class Volume;

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
  void SetCameraAspect(float aspect);
  void SetMatrixTestUniform(const glm::vec3& value);
  glm::vec3 GetMatrixTestUniform() const;
  void CollectInspectableFields(std::vector<UiField>& out);

private:
  float clearColor[4];

  PerspectiveCamera camera;
  CompositeUniformProvider frameUniforms;

  std::shared_ptr<Skybox> skybox;

  std::vector<std::shared_ptr<Light>> lights;
  std::vector<std::shared_ptr<Volume>> volumes;
  std::vector<std::shared_ptr<GameObject>> gameObjects;
  std::map<std::string, std::shared_ptr<Shader>> shaders;

  glm::vec3 matrixTestUniformValue;
};
