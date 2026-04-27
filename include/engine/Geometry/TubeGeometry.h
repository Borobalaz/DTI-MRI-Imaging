#pragma once

#include <vector>
#include "Geometry/Geometry.h"

class TubeGeometry final : public Geometry
{
public:
  TubeGeometry(std::vector<Vertex> vertices,
               std::vector<unsigned int> indices,
               float radius = 0.02f,
               unsigned int radialSegments = 8);

  void Generate() override;

private:
  float radius;
  unsigned int radialSegments;
};