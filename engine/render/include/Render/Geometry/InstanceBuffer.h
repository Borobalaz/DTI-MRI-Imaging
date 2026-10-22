#pragma once

#include <vector>

#include <glm/glm.hpp>

// RAII wrapper around a single GL_ARRAY_BUFFER of glm::mat4, one per instance.
// Exclusively owned by the GameObject that enables instancing - never shared,
// since Geometry itself may be shared (via shared_ptr) across multiple GameObjects.
class InstanceBuffer
{
  friend class RenderBackend;

public:
  InstanceBuffer() = default;
  ~InstanceBuffer();

  InstanceBuffer(const InstanceBuffer&) = delete;
  InstanceBuffer& operator=(const InstanceBuffer&) = delete;
  InstanceBuffer(InstanceBuffer&& other) noexcept;
  InstanceBuffer& operator=(InstanceBuffer&& other) noexcept;

  // (Re)uploads the full instance-transform list (GL_DYNAMIC_DRAW), creating
  // the GL buffer lazily on first call. An empty list resets to not-instanced.
  // Must run on the engine/GL thread, same constraint as Geometry::Upload.
  void Upload(const std::vector<glm::mat4>& transforms);

  void Reset(); // deletes the GL buffer, returns to the empty/not-instanced state

  bool IsValid() const { return vbo != 0 && instanceCount > 0; }
  size_t GetInstanceCount() const { return instanceCount; }

private:
  unsigned int GetHandle() const { return vbo; }

  unsigned int vbo = 0;
  size_t instanceCount = 0;
};
