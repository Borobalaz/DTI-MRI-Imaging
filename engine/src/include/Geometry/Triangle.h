#pragma once

#include "Geometry/Geometry.h"

class Triangle : public Geometry
{
public:
  Triangle();
  void Generate() override;
};