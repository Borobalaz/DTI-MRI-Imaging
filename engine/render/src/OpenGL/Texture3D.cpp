#include "Render/Texture/Texture3D.h"

#include <iostream>
#include <string>
#include <utility>

#include "foundation/log/EngineLog.h"
#include "OpenGL/GpuDeletionQueue.h"
#include "OpenGL/TextureGL.h"

namespace
{
  std::string DescribeTextureFormat(TextureFormat format)
  {
    switch (format)
    {
      case TextureFormat::R32F:
        return "R32F";
      case TextureFormat::RGB32F:
        return "RGB32F";
      case TextureFormat::RGBA32F:
        return "RGBA32F";
      case TextureFormat::RGBA8:
        return "RGBA8";
      case TextureFormat::Depth24:
        return "Depth24";
    }
    return "unknown";
  }

  std::string DescribeTexture(const glm::ivec3& dimensions, TextureFormat format)
  {
    return std::to_string(dimensions.x) + "x" + std::to_string(dimensions.y) + "x" +
           std::to_string(dimensions.z) + " " + DescribeTextureFormat(format);
  }
}

Texture3D::Texture3D(int width,
                     int height,
                     int depth,
                     TextureFormat format,
                     const void* data,
                     bool linearFiltering)
  : id(0),
    residency(Residency::Failed),
    linearFiltering(linearFiltering)
{
  if (UploadNow(glm::ivec3(width, height, depth), format, data, linearFiltering))
  {
    residency = Residency::Resident;
  }
  glBindTexture(GL_TEXTURE_3D, 0);
}

Texture3D::Texture3D(TextureData3D data, bool linearFiltering)
  : id(0),
    residency(Residency::Failed),
    linearFiltering(linearFiltering)
{
  if (!data.IsWellFormed())
  {
    EngineLog::Error("Malformed 3D texture data: " + DescribeTexture(data.dimensions, data.format) + ", " +
                      std::to_string(data.texels.size()) + " floats", "Texture3D");
    return;
  }

  pending = std::move(data);
  residency = Residency::Pending;
}

// May run on any thread (e.g. the UI thread releasing the last owner), so the
// handle is queued for deletion on the GL thread instead of deleted here.
Texture3D::~Texture3D()
{
  EnqueueGpuDeletion(GpuResourceKind::Texture, id);
}

bool Texture3D::UploadNow(const glm::ivec3& dimensions, TextureFormat format, const void* texels, bool linear) const
{
  // Drain errors left by earlier GL calls so the check below only reflects this upload; the lazy
  // path runs mid-frame, where unrelated errors may still be queued.
  while (glGetError() != GL_NO_ERROR)
  {
  }

  GLint maxTextureSize = 0;
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maxTextureSize);

  if (dimensions.x <= 0 || dimensions.y <= 0 || dimensions.z <= 0 ||
      dimensions.x > maxTextureSize || dimensions.y > maxTextureSize || dimensions.z > maxTextureSize)
  {
    std::cout << "Invalid 3D texture size: " << dimensions.x << "x" << dimensions.y << "x" << dimensions.z << std::endl;
    EngineLog::Error("Invalid 3D texture size: " + DescribeTexture(dimensions, format), "Texture3D");
    return false;
  }

  glGenTextures(1, &id);
  if (id == 0)
  {
    std::cout << "Failed to allocate 3D texture." << std::endl;
    EngineLog::Error("Failed to allocate 3D texture.", "Texture3D");
    return false;
  }

  glBindTexture(GL_TEXTURE_3D, id);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);

  const GLTextureFormat glFormat = ToGLTextureFormat(format);
  glTexImage3D(GL_TEXTURE_3D, 0, glFormat.internalFormat, dimensions.x, dimensions.y, dimensions.z, 0,
               glFormat.format, glFormat.type, texels);

  if (glGetError() != GL_NO_ERROR)
  {
    EngineLog::Error("Failed to upload 3D texture: " + DescribeTexture(dimensions, format), "Texture3D");
    return false;
  }
  return true;
}

void Texture3D::EnsureGpuResident() const
{
  residency = UploadNow(pending.dimensions, pending.format, pending.texels.data(), linearFiltering)
      ? Residency::Resident
      : Residency::Failed;

  // The GPU copy (or the failure) is final either way, so the CPU texels are no longer needed.
  std::vector<float>().swap(pending.texels);
}

void Texture3D::Bind(unsigned int unit) const
{
  glActiveTexture(GL_TEXTURE0 + unit);

  if (residency == Residency::Pending)
  {
    EnsureGpuResident();
  }

  // Leaves the unit empty rather than holding a stale texture from an earlier draw.
  glBindTexture(GL_TEXTURE_3D, residency == Residency::Resident ? id : 0);
}

bool Texture3D::IsValid() const
{
  return residency != Residency::Failed;
}

unsigned int Texture3D::GetId() const
{
  return residency == Residency::Resident ? id : 0;
}
