#pragma once

#include "Geometry.h"

class SphereGeometry : public Geometry
{
public:
  SphereGeometry(float radius = 1.0f, unsigned int sectors = 64, unsigned int stacks = 32);
  ~SphereGeometry() override;

  void Generate() override;

private:
  float radius;
  unsigned int sectors;
  unsigned int stacks;
};
