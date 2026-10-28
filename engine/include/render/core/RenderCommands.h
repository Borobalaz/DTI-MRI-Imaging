#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "render/uniform/CompositeUniformProvider.h"

class Shader;
class Geometry;
class InstanceBuffer;
class Material;
class TextureCube;
class VolumeTextureSet;

// Typed render commands: the boundary data the scene side appends to a
// RenderDataStore (via RenderProxy) and the render passes consume.

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
  const InstanceBuffer* instances = nullptr; // non-owning; owned by the GameObject
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
  std::shared_ptr<Geometry> geometry;
  std::shared_ptr<Shader> shader;
  std::shared_ptr<VolumeTextureSet> textures;
  glm::ivec3 dimensions{0};
  glm::mat4 modelMatrix{1.0f};
  glm::mat4 inverseModelMatrix{1.0f};
  CompositeUniformProvider frameUniforms;
};

struct SkyboxRenderCommand
{
  std::shared_ptr<Geometry> geometry;
  std::shared_ptr<Shader> shader;
  std::shared_ptr<TextureCube> cubemap;
  CompositeUniformProvider frameUniforms;
};
