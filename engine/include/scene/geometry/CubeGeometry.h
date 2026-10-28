#pragma once

#include "render/geometry/Geometry.h"

class CubeGeometry : public Geometry
{
public:
  CubeGeometry();

  void Generate() override;
};