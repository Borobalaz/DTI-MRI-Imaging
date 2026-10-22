#pragma once

#include "Render/Geometry/Geometry.h"

class CubeGeometry : public Geometry
{
public:
  CubeGeometry();

  void Generate() override;
};