#pragma once

#include <vector>

#include "Render/Texture/Texture.h"
#include "Render/TextureFormat.h"

// 1D float texel buffer sampled as a samplerBuffer (GL 3.1 buffer texture), for per-instance or
// per-vertex payloads too large for vertex attributes. Supports R32F and RGBA32F only (RGB32F
// buffer textures need GL 4.0).
//
// Graphics-API free until bound, so it may be built and filled on any thread: the buffer and
// texture objects are created, and the texels uploaded, on the GL thread at the first Bind()
// after construction or SetData(). The CPU texels are released once uploaded.
class BufferTexture : public Texture
{
public:
  BufferTexture(TextureFormat format, std::vector<float> texels);
  ~BufferTexture() override;

  BufferTexture(const BufferTexture&) = delete;
  BufferTexture& operator=(const BufferTexture&) = delete;

  // Replaces the texels; they are re-uploaded at the next Bind(). Like construction, must not
  // race a Bind() on the GL thread.
  void SetData(std::vector<float> texels);

  // Pending uploads count as valid; a failure (unsupported format, malformed or oversized data)
  // surfaces at construction/SetData or at the first Bind() after it.
  bool IsValid() const override;

private:
  enum class Residency
  {
    Resident,
    Pending,
    Failed
  };

  void Bind(unsigned int unit) const override;
  unsigned int GetId() const override;

  void EnsureGpuResident() const;
  // Creates the buffer/texture objects on first use, (re)fills the buffer and attaches it to the
  // texture, leaving the texture bound on the active unit.
  bool UploadNow() const;

  // Float components per texel for the supported formats (1/4), 0 for unsupported ones.
  static int ComponentCount(TextureFormat format);

  TextureFormat format;
  mutable unsigned int bufferId = 0;
  mutable unsigned int textureId = 0;
  mutable Residency residency = Residency::Failed;
  mutable std::vector<float> pending;
};
