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

class Geometry
{
  friend class RenderBackend;

public:
  Geometry();
  virtual ~Geometry();

  // Core lifecycle
  virtual void Generate() = 0;
  virtual void Upload();
  virtual void Destroy();

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

  unsigned int VAO;
  unsigned int VBO;
  unsigned int EBO;

  PrimitiveTopology primitive = PrimitiveTopology::Triangles;
  bool hasPrimitiveRestart = false;
  unsigned int primitiveRestartIndex = 0;
  float lineWidth = 1.0f;
};