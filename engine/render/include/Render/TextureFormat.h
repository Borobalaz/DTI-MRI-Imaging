#pragma once

// Graphics-API-neutral pixel storage formats for texture allocation. Each value
// maps to a fixed (internal format, pixel format, component type) triple on the
// GL side (see ToGLTextureFormat in engine/render/src/OpenGL/TextureGL.h).
enum class TextureFormat
{
  R32F,
  RGB32F,
  RGBA32F,
  RGBA8,
  Depth24
};
