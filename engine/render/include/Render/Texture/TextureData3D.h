#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "Render/TextureFormat.h"

// CPU-side texel payload for a 3D float texture. Graphics-API free, so it can be built and
// filled on any thread (e.g. a preprocessing worker) and handed to the lazy Texture3D ctor,
// which uploads it on the GL thread at first bind.
struct TextureData3D
{
  glm::ivec3 dimensions{0};
  TextureFormat format = TextureFormat::R32F; // R32F | RGB32F | RGBA32F only
  std::vector<float> texels;                  // x-fastest, then y, then z; components interleaved

  // Float components per texel for the supported formats (1/3/4), 0 for unsupported ones.
  static int ComponentCount(TextureFormat format);

  // Positive dimensions, a supported format and exactly width * height * depth * components texels.
  bool IsWellFormed() const;
};
