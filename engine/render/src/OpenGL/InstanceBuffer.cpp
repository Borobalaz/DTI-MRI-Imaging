#include "Render/Geometry/InstanceBuffer.h"

#include <glad/glad.h>

#include <utility>

#include "OpenGL/GpuDeletionQueue.h"

InstanceBuffer::~InstanceBuffer()
{
  ReleaseDeferred();
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
    ReleaseDeferred();
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

void InstanceBuffer::ReleaseDeferred()
{
  EnqueueGpuDeletion(GpuResourceKind::Buffer, vbo);
  vbo = 0;
  instanceCount = 0;
}
