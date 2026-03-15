#pragma once

#include <vector>

#include <glm/glm.hpp>
#include "Shader.h"

class Shader;

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texCoord;
};

class Geometry
{
public:
  Geometry();
  virtual ~Geometry();

  // Core lifecycle
  virtual void Generate() = 0;
  virtual void Upload();
  virtual void Draw(Shader& shader) const;
  virtual void Destroy();

  // Geometry processing
  void ComputeNormals();

protected:
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  unsigned int VAO;
  unsigned int VBO;
  unsigned int EBO;
};