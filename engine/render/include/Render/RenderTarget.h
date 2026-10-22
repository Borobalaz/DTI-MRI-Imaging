#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class Framebuffer;

// An offscreen color + depth render target the renderer can draw a frame into and
// the host can read back as CPU pixels. Construction is GL-free (so a target can be
// created from any thread); everything else runs on the GL thread. Binding is GPU-only
// and goes through RenderBackend::BindTarget.
class RenderTarget
{
  friend class RenderBackend;

public:
  RenderTarget();
  ~RenderTarget();

  RenderTarget(const RenderTarget&) = delete;
  RenderTarget& operator=(const RenderTarget&) = delete;

  // (Re)allocates the GPU storage when the size changed. No-op on non-positive size.
  void Resize(int width, int height);

  int GetWidth() const;
  int GetHeight() const;
  bool IsValid() const;

  // Copies the color attachment into `pixels` as tightly packed RGBA8, rows ordered
  // TOP-DOWN (row 0 is the top of the image), so hosts can display it without flipping.
  // Returns false (leaving `pixels` untouched) when the target has no storage yet.
  bool ReadPixelsRGBA8(std::vector<std::uint8_t>& pixels) const;

private:
  // GPU-only: binds the target as the draw framebuffer and sets the viewport to match.
  void Bind() const;

  std::unique_ptr<Framebuffer> framebuffer;
};
