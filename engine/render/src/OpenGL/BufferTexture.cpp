#include "Render/Texture/BufferTexture.h"

#include <string>
#include <utility>

#include "foundation/log/EngineLog.h"
#include "OpenGL/GpuDeletionQueue.h"
#include "OpenGL/TextureGL.h"

BufferTexture::BufferTexture(TextureFormat format, std::vector<float> texels)
  : format(format)
{
  SetData(std::move(texels));
}

// May run on any thread (e.g. a worker dropping a superseded instance buffer), so the handles
// are queued for deletion on the GL thread instead of deleted here.
BufferTexture::~BufferTexture()
{
  EnqueueGpuDeletion(GpuResourceKind::Texture, textureId);
  EnqueueGpuDeletion(GpuResourceKind::Buffer, bufferId);
}

void BufferTexture::SetData(std::vector<float> texels)
{
  const int components = ComponentCount(format);
  if (components == 0)
  {
    EngineLog::Error("Unsupported buffer texture format (only R32F and RGBA32F are supported).", "BufferTexture");
    residency = Residency::Failed;
    std::vector<float>().swap(pending);
    return;
  }

  if (texels.size() % static_cast<size_t>(components) != 0)
  {
    EngineLog::Error("Malformed buffer texture data: " + std::to_string(texels.size()) +
                     " floats is not a multiple of " + std::to_string(components) + " components",
                     "BufferTexture");
    residency = Residency::Failed;
    std::vector<float>().swap(pending);
    return;
  }

  pending = std::move(texels);
  residency = Residency::Pending;
}

int BufferTexture::ComponentCount(TextureFormat format)
{
  switch (format)
  {
    case TextureFormat::R32F:
      return 1;
    case TextureFormat::RGBA32F:
      return 4;
    default:
      return 0;
  }
}

bool BufferTexture::UploadNow() const
{
  // Drain errors left by earlier GL calls so the check below only reflects this upload; the lazy
  // path runs mid-frame, where unrelated errors may still be queued.
  while (glGetError() != GL_NO_ERROR)
  {
  }

  const size_t texelCount = pending.size() / static_cast<size_t>(ComponentCount(format));
  GLint maxTexelCount = 0;
  glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTexelCount);
  if (texelCount > static_cast<size_t>(maxTexelCount))
  {
    EngineLog::Error("Buffer texture too large: " + std::to_string(texelCount) + " texels exceeds " +
                     "GL_MAX_TEXTURE_BUFFER_SIZE (" + std::to_string(maxTexelCount) + ")", "BufferTexture");
    return false;
  }

  if (bufferId == 0)
  {
    glGenBuffers(1, &bufferId);
  }
  if (textureId == 0)
  {
    glGenTextures(1, &textureId);
  }
  if (bufferId == 0 || textureId == 0)
  {
    EngineLog::Error("Failed to allocate buffer texture.", "BufferTexture");
    return false;
  }

  glBindBuffer(GL_TEXTURE_BUFFER, bufferId);
  glBufferData(GL_TEXTURE_BUFFER, static_cast<GLsizeiptr>(pending.size() * sizeof(float)), pending.data(),
               GL_STATIC_DRAW);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);

  glBindTexture(GL_TEXTURE_BUFFER, textureId);
  glTexBuffer(GL_TEXTURE_BUFFER, ToGLTextureFormat(format).internalFormat, bufferId);

  if (glGetError() != GL_NO_ERROR)
  {
    EngineLog::Error("Failed to upload buffer texture: " + std::to_string(texelCount) + " texels", "BufferTexture");
    return false;
  }
  return true;
}

void BufferTexture::EnsureGpuResident() const
{
  residency = UploadNow() ? Residency::Resident : Residency::Failed;

  // The GPU copy (or the failure) is final until the next SetData, so the CPU texels are no longer needed.
  std::vector<float>().swap(pending);
}

void BufferTexture::Bind(unsigned int unit) const
{
  glActiveTexture(GL_TEXTURE0 + unit);

  if (residency == Residency::Pending)
  {
    EnsureGpuResident();
  }

  // Leaves the unit empty rather than holding a stale texture from an earlier draw.
  glBindTexture(GL_TEXTURE_BUFFER, residency == Residency::Resident ? textureId : 0);
}

bool BufferTexture::IsValid() const
{
  return residency != Residency::Failed;
}

unsigned int BufferTexture::GetId() const
{
  return residency == Residency::Resident ? textureId : 0;
}
