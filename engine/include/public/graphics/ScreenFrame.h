#pragma once

#include <cstdint>
#include <vector>

// Identifies an engine-owned screen (an offscreen render target the host displays).
// 0 is never a valid screen.
using ScreenId = std::uint32_t;
constexpr ScreenId kInvalidScreenId = 0;

/**
 * @brief One rendered frame of a screen, read back to CPU memory.
 *
 *  `pixels` is tightly packed RGBA8 (width * height * 4 bytes, no row padding), rows ordered
 *  TOP-DOWN: row 0 is the top of the image, so hosts can display it without flipping.
 */
struct ScreenFrame
{
  int width = 0;
  int height = 0;
  std::uint64_t frameIndex = 0; // engine tick that produced the frame; increases monotonically
  double frameTimeMs = 0.0;     // engine-side cost of the tick that produced it (update + render + readback)
  double fps = 0.0;             // engine tick rate measured at that tick
  std::vector<std::uint8_t> pixels;
};
