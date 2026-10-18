#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Uniform/CompositeUniformProvider.h"
#include "RenderCore/RenderDataStore.h"

class Shader;
class Geometry;
class Material;
class Volume;
class Skybox;
class VolumeTextureSet;
class VolumeGeometry;
class CubeGeometry;

struct MeshRenderCommand
{
  std::shared_ptr<Geometry> geometry;
  std::shared_ptr<Material> material;
  glm::mat4 modelMatrix{1.0f};
  CompositeUniformProvider frameUniforms;
};

struct MeshRenderBatch
{
  std::vector<MeshRenderCommand> draws;
};

struct MeshInstancedRenderCommand
{
  std::shared_ptr<Geometry> geometry;
  std::shared_ptr<Material> material;
  unsigned int instanceVBO = 0;   // non-owning handle; owned by the GameObject's InstanceBuffer
  size_t instanceCount = 0;
  // Outer/group transform shared by every instance (the GameObject's own transform), combined
  // in the shader with each instance's own matrix as modelMatrix * instanceMatrix. Lets the
  // whole instanced batch be moved/rotated as a group (e.g. animated each frame) without
  // re-uploading the per-instance buffer.
  glm::mat4 modelMatrix{1.0f};
  CompositeUniformProvider frameUniforms;
};

struct MeshInstancedRenderBatch
{
  std::vector<MeshInstancedRenderCommand> draws;
};

struct VolumeRenderCommand
{
  std::shared_ptr<VolumeGeometry> geometry;
  std::shared_ptr<Shader> shader;
  std::shared_ptr<VolumeTextureSet> textures;
  glm::ivec3 dimensions{0};
  glm::mat4 modelMatrix{1.0f};
  glm::mat4 inverseModelMatrix{1.0f};
  CompositeUniformProvider frameUniforms;
};

struct SkyboxRenderCommand
{
  std::shared_ptr<CubeGeometry> geometry;
  std::shared_ptr<Shader> shader;
  std::shared_ptr<class TextureCube> cubemap;
  CompositeUniformProvider frameUniforms;
};

struct RenderProxy
{
  RenderDataStore data;
  CompositeUniformProvider frameUniforms;
  bool visible = true;
};
