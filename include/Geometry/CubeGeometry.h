#pragma once

#include "Geometry.h"

class CubeGeometry : public Geometry
{
public:
  CubeGeometry();

  void Generate() override;
};