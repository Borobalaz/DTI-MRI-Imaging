#include "Render/RenderTarget.h"

#include <algorithm>
#include <cstddef>

#include <glad/glad.h>

#include "OpenGL/Framebuffer.h"

RenderTarget::RenderTarget()
  : framebuffer(std::make_unique<Framebuffer>(true))
{
}

// Out of line so the unique_ptr<Framebuffer> member is destroyed where Framebuffer is complete.
RenderTarget::~RenderTarget() = default;

void RenderTarget::Resize(int width, int height)
{
  framebuffer->Resize(width, height);
}

int RenderTarget::GetWidth() const
{
  return framebuffer->GetWidth();
}

int RenderTarget::GetHeight() const
{
  return framebuffer->GetHeight();
}

bool RenderTarget::IsValid() const
{
  return framebuffer->IsValid();
}

void RenderTarget::Bind() const
{
  framebuffer->Bind();
}

bool RenderTarget::ReadPixelsRGBA8(std::vector<std::uint8_t>& pixels) const
{
  if (!framebuffer->IsValid())
  {
    return false;
  }

  const int width = framebuffer->GetWidth();
  const int height = framebuffer->GetHeight();
  const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
  pixels.resize(rowBytes * static_cast<std::size_t>(height));

  framebuffer->Bind();
  // RGBA8 rows are always a multiple of 4 bytes, so alignment 4 means no row padding.
  glPixelStorei(GL_PACK_ALIGNMENT, 4);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

  // GL returns rows bottom-up; reverse them in place so the published image is top-down.
  for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom)
  {
    std::uint8_t* topRow = pixels.data() + static_cast<std::size_t>(top) * rowBytes;
    std::uint8_t* bottomRow = pixels.data() + static_cast<std::size_t>(bottom) * rowBytes;
    std::swap_ranges(topRow, topRow + rowBytes, bottomRow);
  }

  return true;
}
