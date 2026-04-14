#pragma once

#include <limits>
#include <vector>

#include "Geometry.h"

class StreamlineGeometry final : public Geometry
{
public:
  static constexpr unsigned int kRestartIndex = std::numeric_limits<unsigned int>::max();

  StreamlineGeometry(std::vector<Vertex> vertices,
                     std::vector<unsigned int> indices);

  void Generate() override;
  void Draw(Shader &shader) const override;
};
