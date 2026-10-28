#pragma once

#include <string>

#include "scene/glyphs/SHVolume.h"

// Binary (.shb) read/write of an SHVolume's dense coefficients (not its mask).
// Both throw std::runtime_error with a message naming filePath on any failure.
namespace SHVolumeFileIO
{
  void Save(const std::string& filePath, const SHVolume& volume);
  SHVolume Load(const std::string& filePath);   // the returned volume has mask == nullptr
}
