#include "render/texture/TextureData3D.h"

int TextureData3D::ComponentCount(TextureFormat format)
{
  switch (format)
  {
    case TextureFormat::R32F:
      return 1;
    case TextureFormat::RGB32F:
      return 3;
    case TextureFormat::RGBA32F:
      return 4;
    default:
      return 0;
  }
}

bool TextureData3D::IsWellFormed() const
{
  const int componentCount = ComponentCount(format);
  if (dimensions.x <= 0 || dimensions.y <= 0 || dimensions.z <= 0 || componentCount == 0)
  {
    return false;
  }

  const size_t expectedTexelCount = static_cast<size_t>(dimensions.x) *
                                    static_cast<size_t>(dimensions.y) *
                                    static_cast<size_t>(dimensions.z) *
                                    static_cast<size_t>(componentCount);
  return texels.size() == expectedTexelCount;
}
