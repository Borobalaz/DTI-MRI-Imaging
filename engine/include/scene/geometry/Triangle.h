#pragma once

#include "render/geometry/Geometry.h"

class Triangle : public Geometry
{
public:
  Triangle();
  void Generate() override;
};