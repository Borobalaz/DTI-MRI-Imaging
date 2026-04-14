#include "StreamlineGeometry.h"

#include <utility>

#include <glad/glad.h>

StreamlineGeometry::StreamlineGeometry(std::vector<Vertex> vertices,
                                       std::vector<unsigned int> indices)
{
  this->vertices = std::move(vertices);
  this->indices = std::move(indices);
  Upload();
}

void StreamlineGeometry::Generate()
{
}

void StreamlineGeometry::Draw(Shader &shader) const
{
  shader.Use();

  glBindVertexArray(VAO);
  glLineWidth(1.5f);
  glEnable(GL_PRIMITIVE_RESTART);
  glPrimitiveRestartIndex(kRestartIndex);
  glDrawElements(
      GL_LINE_STRIP,
      static_cast<GLsizei>(indices.size()),
      GL_UNSIGNED_INT,
      nullptr);
  glDisable(GL_PRIMITIVE_RESTART);
  glBindVertexArray(0);
}
