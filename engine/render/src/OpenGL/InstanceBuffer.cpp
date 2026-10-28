#include "Render/Geometry/InstanceBuffer.h"

#include <glad/glad.h>

#include <string>
#include <utility>

#include "Log/EngineLog.h"
#include "OpenGL/GpuDeletionQueue.h"

namespace
{
  constexpr unsigned int kFirstAttributeStreamLocation = 8;
  constexpr unsigned int kLastAttributeStreamLocation = 15;

  // Generates the buffer on first use and refills it (GL_DYNAMIC_DRAW); leaves GL_ARRAY_BUFFER unbound.
  void UploadArrayBuffer(unsigned int& vbo, const void* bytes, size_t byteCount)
  {
    if (vbo == 0)
    {
      glGenBuffers(1, &vbo);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(byteCount), bytes, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
}

InstanceBuffer::~InstanceBuffer()
{
  ReleaseDeferred();
}

// std::exchange leaves `other` empty, so it no longer owns (and won't release) any handle.
InstanceBuffer::InstanceBuffer(InstanceBuffer&& other) noexcept
  : transforms(std::exchange(other.transforms, {})),
    transformVbo(std::exchange(other.transformVbo, 0u)),
    transformsGpuDirty(std::exchange(other.transformsGpuDirty, false)),
    attributeStreams(std::exchange(other.attributeStreams, {})),
    bufferStreams(std::exchange(other.bufferStreams, {})),
    streamSlots(std::exchange(other.streamSlots, {})),
    attributeBindings(std::exchange(other.attributeBindings, {})),
    hasDescriptorError(std::exchange(other.hasDescriptorError, false))
{
}

InstanceBuffer& InstanceBuffer::operator=(InstanceBuffer&& other) noexcept
{
  if (this != &other)
  {
    ReleaseDeferred();
    transforms = std::exchange(other.transforms, {});
    transformVbo = std::exchange(other.transformVbo, 0u);
    transformsGpuDirty = std::exchange(other.transformsGpuDirty, false);
    attributeStreams = std::exchange(other.attributeStreams, {});
    bufferStreams = std::exchange(other.bufferStreams, {});
    streamSlots = std::exchange(other.streamSlots, {});
    attributeBindings = std::exchange(other.attributeBindings, {});
    hasDescriptorError = std::exchange(other.hasDescriptorError, false);
  }
  return *this;
}

void InstanceBuffer::SetTransforms(std::vector<glm::mat4> newTransforms)
{
  transforms = std::move(newTransforms);
  transformsGpuDirty = true;
}

InstanceStreamId InstanceBuffer::AddAttributeStream(const InstanceAttributeStreamDesc& desc)
{
  if (desc.location < kFirstAttributeStreamLocation || desc.location > kLastAttributeStreamLocation)
  {
    return ReportDescriptorError("Instance attribute stream location " + std::to_string(desc.location) +
                                 " is outside 8..15");
  }
  if (desc.components < 1 || desc.components > 4)
  {
    return ReportDescriptorError("Instance attribute stream components must be 1..4, got " +
                                 std::to_string(desc.components));
  }
  for (const AttributeStream& existing : attributeStreams)
  {
    if (existing.desc.location == desc.location)
    {
      return ReportDescriptorError("Instance attribute stream location " + std::to_string(desc.location) +
                                   " is already in use");
    }
  }

  AttributeStream stream;
  stream.desc = desc;
  attributeStreams.push_back(std::move(stream));
  streamSlots.push_back({false, attributeStreams.size() - 1});
  return streamSlots.size() - 1;
}

InstanceStreamId InstanceBuffer::AddBufferStream(const InstanceBufferStreamDesc& desc)
{
  if (bufferStreams.size() >= kMaxBufferStreams)
  {
    return ReportDescriptorError("At most " + std::to_string(kMaxBufferStreams) +
                                 " instance buffer streams are supported");
  }

  size_t componentsPerTexel = 0;
  if (desc.format == TextureFormat::R32F)
  {
    componentsPerTexel = 1;
  }
  else if (desc.format == TextureFormat::RGBA32F)
  {
    componentsPerTexel = 4;
  }
  else
  {
    return ReportDescriptorError("Instance buffer stream '" + desc.samplerName +
                                 "' must use R32F or RGBA32F");
  }

  if (desc.floatsPerInstance == 0 || desc.floatsPerInstance % componentsPerTexel != 0)
  {
    return ReportDescriptorError("Instance buffer stream '" + desc.samplerName +
                                 "' floatsPerInstance must be a positive multiple of the texel size");
  }

  BufferStream stream;
  stream.desc = desc;
  stream.texture = std::make_unique<BufferTexture>(desc.format, std::vector<float>());
  bufferStreams.push_back(std::move(stream));
  streamSlots.push_back({true, bufferStreams.size() - 1});
  return streamSlots.size() - 1;
}

void InstanceBuffer::SetStreamData(InstanceStreamId stream, std::vector<float> values)
{
  if (stream >= streamSlots.size())
  {
    ReportDescriptorError(stream == kInvalidInstanceStream
                            ? std::string("SetStreamData called with the invalid stream id")
                            : "Unknown instance stream id " + std::to_string(stream));
    return;
  }

  const StreamSlot& slot = streamSlots[stream];
  if (slot.isBufferStream)
  {
    BufferStream& bufferStream = bufferStreams[slot.index];
    bufferStream.floatCount = values.size();
    bufferStream.texture->SetData(std::move(values));
    return;
  }

  AttributeStream& attributeStream = attributeStreams[slot.index];
  attributeStream.values = std::move(values);
  attributeStream.gpuDirty = true;
}

void InstanceBuffer::Clear()
{
  ReleaseDeferred();
}

InstanceStreamId InstanceBuffer::ReportDescriptorError(const std::string& message)
{
  EngineLog::Error(message, "InstanceBuffer");
  hasDescriptorError = true;
  return kInvalidInstanceStream;
}

bool InstanceBuffer::IsValid() const
{
  const size_t instanceCount = transforms.size();
  if (instanceCount == 0 || hasDescriptorError)
  {
    return false;
  }

  for (const AttributeStream& stream : attributeStreams)
  {
    if (stream.values.size() != instanceCount * static_cast<size_t>(stream.desc.components))
    {
      return false;
    }
  }
  for (const BufferStream& stream : bufferStreams)
  {
    if (stream.floatCount != instanceCount * stream.desc.floatsPerInstance || !stream.texture->IsValid())
    {
      return false;
    }
  }
  return true;
}

void InstanceBuffer::Apply(IUniformSink& sink) const
{
  for (size_t i = 0; i < bufferStreams.size(); ++i)
  {
    const BufferStream& stream = bufferStreams[i];
    sink.SetTexture(stream.desc.samplerName, stream.texture.get(),
                    kFirstBufferStreamTextureUnit + static_cast<unsigned int>(i));
  }
}

void InstanceBuffer::EnsureGpuResident() const
{
  if (transformVbo == 0 || transformsGpuDirty)
  {
    UploadArrayBuffer(transformVbo, transforms.data(), transforms.size() * sizeof(glm::mat4));
    transformsGpuDirty = false;
  }

  attributeBindings.clear();
  for (const AttributeStream& stream : attributeStreams)
  {
    if (stream.vbo == 0 || stream.gpuDirty)
    {
      UploadArrayBuffer(stream.vbo, stream.values.data(), stream.values.size() * sizeof(float));
      stream.gpuDirty = false;
    }
    attributeBindings.push_back({stream.vbo, stream.desc.location, stream.desc.components});
  }
}

void InstanceBuffer::ReleaseDeferred()
{
  EnqueueGpuDeletion(GpuResourceKind::Buffer, transformVbo);
  transformVbo = 0;
  for (const AttributeStream& stream : attributeStreams)
  {
    EnqueueGpuDeletion(GpuResourceKind::Buffer, stream.vbo);
  }

  // BufferTexture destructors queue their own handles.
  transforms.clear();
  transformsGpuDirty = false;
  attributeStreams.clear();
  bufferStreams.clear();
  streamSlots.clear();
  attributeBindings.clear();
  hasDescriptorError = false;
}
