#pragma once

#include "Render/Geometry/Geometry.h"

class Triangle : public Geometry
{
public:
  Triangle();
  void Generate() override;
};