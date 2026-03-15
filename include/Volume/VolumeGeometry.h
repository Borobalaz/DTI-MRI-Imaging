#pragma once

#include "Geometry.h"

class VolumeGeometry : public Geometry
{
public:
  VolumeGeometry();

  void Generate() override;
};
