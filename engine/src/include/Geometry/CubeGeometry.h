#pragma once

#include "Geometry/Geometry.h"

class CubeGeometry : public Geometry
{
public:
  CubeGeometry();

  void Generate() override;
};