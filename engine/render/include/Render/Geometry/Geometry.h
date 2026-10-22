#pragma once

#include <vector>

#include <glm/glm.hpp>
#include "Render/Shader.h"

class Shader;
class InstanceBuffer;

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texCoord;
  glm::vec3 tangent;  // tangent direction for line-based geometry
};

enum class PrimitiveTopology
{
  Triangles,
  LineStrip
};

// Construction is GL-free, so geometry can be built on any thread (e.g. a
// preprocessing worker). GPU buffers are created lazily on the first draw, on
// the GL thread. The GPU upload only reads vertices/indices; once a Geometry has
// been handed off to the GL thread, nothing may mutate them (that would be a
// data race with the upload and with off-thread readers such as dataset saving).
class Geometry
{
  friend class RenderBackend;

public:
  Geometry();
  virtual ~Geometry();

  // Core lifecycle
  virtual void Generate() = 0;

  // Requests a re-upload of vertices/indices on the next draw. Call on the GL
  // thread (or before handoff) after changing the CPU-side data.
  void MarkGpuDirty();

  // Geometry processing
  void ComputeNormals();

  const std::vector<Vertex>& GetVertices() const { return vertices; }
  const std::vector<unsigned int>& GetIndices() const { return indices; }

protected:
  // Draw-mode configuration consumed by Draw(); set by subclasses that are not plain triangle meshes.
  void SetPrimitive(PrimitiveTopology topology);
  void SetPrimitiveRestartIndex(unsigned int restartIndex);
  void SetLineWidth(float width);

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

private:
  // GPU-only: reachable through RenderBackend::Draw / RenderBackend::DrawInstanced.
  void Draw(Shader& shader) const;
  void DrawInstanced(Shader& shader, const InstanceBuffer& instances) const;
  void DrawLineStrip() const;

  // Creates the GPU buffers on first use and re-buffers them when dirty.
  void EnsureGpuResident() const;
  void ConfigureVertexAttributes() const;

  // Mutable: GPU residency is a cache of the CPU data, materialised from const Draw().
  mutable unsigned int VAO;
  mutable unsigned int VBO;
  mutable unsigned int EBO;
  mutable bool gpuDirty = false;

  PrimitiveTopology primitive = PrimitiveTopology::Triangles;
  bool hasPrimitiveRestart = false;
  unsigned int primitiveRestartIndex = 0;
  float lineWidth = 1.0f;
};