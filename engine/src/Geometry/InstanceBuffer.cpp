#include "Geometry/InstanceBuffer.h"

#include <glad/glad.h>

#include <utility>

InstanceBuffer::~InstanceBuffer()
{
  Reset();
}

InstanceBuffer::InstanceBuffer(InstanceBuffer&& other) noexcept
  : vbo(other.vbo),
    instanceCount(other.instanceCount)
{
  other.vbo = 0;
  other.instanceCount = 0;
}

InstanceBuffer& InstanceBuffer::operator=(InstanceBuffer&& other) noexcept
{
  if (this != &other)
  {
    Reset();
    vbo = other.vbo;
    instanceCount = other.instanceCount;
    other.vbo = 0;
    other.instanceCount = 0;
  }
  return *this;
}

void InstanceBuffer::Upload(const std::vector<glm::mat4>& transforms)
{
  if (transforms.empty())
  {
    Reset();
    return;
  }

  if (vbo == 0)
  {
    glGenBuffers(1, &vbo);
  }

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(
    GL_ARRAY_BUFFER,
    static_cast<GLsizeiptr>(transforms.size() * sizeof(glm::mat4)),
    transforms.data(),
    GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  instanceCount = transforms.size();
}

void InstanceBuffer::Reset()
{
  if (vbo != 0)
  {
    glDeleteBuffers(1, &vbo);
  }
  vbo = 0;
  instanceCount = 0;
}
