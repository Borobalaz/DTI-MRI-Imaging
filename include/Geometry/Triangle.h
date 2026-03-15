#pragma once

#include "Geometry.h"

class Triangle : public Geometry
{
public:
  Triangle();
  void Generate() override;
};