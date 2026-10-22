#pragma once

#include <glm/glm.hpp>

#include "Render/Texture/Texture.h"
#include "Render/Texture/TextureData3D.h"
#include "Render/TextureFormat.h"

class Texture3D : public Texture
{
public:
  // Eager: uploads immediately, so it must run on the GL thread.
  Texture3D(int width,
            int height,
            int depth,
            TextureFormat format,
            const void* data,
            bool linearFiltering = true);

  // Lazy: graphics-API free, so it may run on any thread. The texels are uploaded on the GL
  // thread at the first Bind() and released afterwards; a texture that is never bound (e.g. a
  // hidden volume) keeps its CPU texels until it is.
  explicit Texture3D(TextureData3D data, bool linearFiltering = true);
  ~Texture3D() override;

  // A texture still waiting for its first upload counts as valid; an upload failure surfaces
  // at that first Bind(), one frame after construction.
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
  // Creates the texture and uploads `texels` while leaving it bound on the active unit.
  bool UploadNow(const glm::ivec3& dimensions, TextureFormat format, const void* texels, bool linear) const;

  mutable unsigned int id;
  mutable Residency residency;
  mutable TextureData3D pending;
  bool linearFiltering;
};
