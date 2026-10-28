#pragma once

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Render/Texture/BufferTexture.h"
#include "Render/TextureFormat.h"
#include "Render/Uniform/UniformProvider.h"

// Per-instance vertex attribute at a fixed location, read with divisor 1. Locations 0-3 are the
// per-vertex layout and 4-7 the instance transform, so streams use 8..15 with 1..4 floats each.
struct InstanceAttributeStreamDesc
{
  unsigned int location;
  int components;
};

// Per-instance payload too large for attributes, exposed to the shader as a samplerBuffer named
// `samplerName` (R32F or RGBA32F). The shader indexes it by gl_InstanceID * texelsPerInstance.
struct InstanceBufferStreamDesc
{
  std::string samplerName;
  TextureFormat format;
  size_t floatsPerInstance;
};

using InstanceStreamId = size_t;

// Returned by Add*Stream for a rejected descriptor.
constexpr InstanceStreamId kInvalidInstanceStream = std::numeric_limits<size_t>::max();

// GPU-side description of one resident attribute stream, consumed by Geometry::DrawInstanced.
struct InstanceAttributeBinding
{
  unsigned int vbo;
  unsigned int location;
  int components;
};

// Per-instance data for an instanced draw: a mandatory transform list (one glm::mat4 per instance,
// locations 4-7, which defines the instance count) plus optional descriptor-declared streams.
// Exclusively owned by the object that enables instancing - never shared, since Geometry itself
// may be shared (via shared_ptr) across multiple GameObjects.
//
// Graphics-API free like Geometry: setters only store CPU data, so a buffer can be built on any
// thread and moved into place; GPU buffers are created/refilled lazily on the GL thread at draw.
// Once handed to the GL thread it must not be mutated concurrently with a draw.
class InstanceBuffer : public UniformProvider
{
  friend class RenderBackend;

public:
  // Texture units of the buffer streams: 8 + the stream's index among buffer streams (8-11).
  // Units below 8 belong to Material, 12-15 to per-draw providers.
  static constexpr unsigned int kFirstBufferStreamTextureUnit = 8;
  static constexpr size_t kMaxBufferStreams = 4;

  InstanceBuffer() = default;
  ~InstanceBuffer() override;

  InstanceBuffer(const InstanceBuffer&) = delete;
  InstanceBuffer& operator=(const InstanceBuffer&) = delete;
  InstanceBuffer(InstanceBuffer&& other) noexcept;
  InstanceBuffer& operator=(InstanceBuffer&& other) noexcept;

  // Replaces the instance-transform list; its size is the instance count.
  void SetTransforms(std::vector<glm::mat4> transforms);

  // Declare a stream. A descriptor outside the documented limits (location/component range,
  // duplicate location, format, too many buffer streams) is logged, returns kInvalidInstanceStream
  // and leaves the buffer invalid until Clear().
  InstanceStreamId AddAttributeStream(const InstanceAttributeStreamDesc& desc);
  InstanceStreamId AddBufferStream(const InstanceBufferStreamDesc& desc);

  // Replaces a stream's data; must hold GetInstanceCount() * floatsPerInstance floats at draw time.
  // An unknown/invalid id is logged and leaves the buffer invalid until Clear().
  void SetStreamData(InstanceStreamId stream, std::vector<float> values);

  // Drops transforms, streams, their GPU buffers (released deferred, so any thread) and any
  // descriptor error.
  void Clear();

  size_t GetInstanceCount() const { return transforms.size(); }
  // At least one instance, no descriptor error, and every stream sized instanceCount * floatsPerInstance.
  bool IsValid() const;

  // Points each buffer stream's sampler at its texture unit and binds its texture there.
  void Apply(IUniformSink& sink) const override;

private:
  struct AttributeStream
  {
    InstanceAttributeStreamDesc desc;
    std::vector<float> values;
    mutable unsigned int vbo = 0;
    mutable bool gpuDirty = false;
  };

  struct BufferStream
  {
    InstanceBufferStreamDesc desc;
    size_t floatCount = 0;
    std::unique_ptr<BufferTexture> texture;
  };

  struct StreamSlot
  {
    bool isBufferStream;
    size_t index; // into attributeStreams or bufferStreams
  };

  // GPU-only: reachable through RenderBackend.
  // Creates/refills the transform and attribute VBOs that are missing or dirty (GL thread).
  void EnsureGpuResident() const;
  unsigned int GetTransformHandle() const { return transformVbo; }
  const std::vector<InstanceAttributeBinding>& GetAttributeBindings() const { return attributeBindings; }

  // Logs a rejected descriptor/stream id and marks the buffer invalid; returns kInvalidInstanceStream.
  InstanceStreamId ReportDescriptorError(const std::string& message);

  // Queues every GPU buffer for deletion on the GL thread and resets to the empty state; used
  // where the calling thread is unknown (Clear, destructor, move-assignment).
  void ReleaseDeferred();

  std::vector<glm::mat4> transforms;
  mutable unsigned int transformVbo = 0;
  mutable bool transformsGpuDirty = false;

  std::vector<AttributeStream> attributeStreams;
  std::vector<BufferStream> bufferStreams;
  std::vector<StreamSlot> streamSlots; // indexed by InstanceStreamId

  // Rebuilt by EnsureGpuResident from attributeStreams.
  mutable std::vector<InstanceAttributeBinding> attributeBindings;

  // Sticky: set by a rejected descriptor or stream id, reset by Clear().
  bool hasDescriptorError = false;
};
