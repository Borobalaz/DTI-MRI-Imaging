#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "Geometry/Geometry.h"

struct MeshFileHeader
{
  char magic[4] = {'M', 'S', 'H', '1'};
  uint32_t version = 1;
  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;
};

class MeshFileLoader
{
public:
  static bool Save(const std::string &filePath, const Geometry &geometry);
  static std::shared_ptr<Geometry> Load(const std::string &filePath);
  static std::string GetLastError();
};
