#pragma once

#include "Geometry/Geometry.h"

class VolumeGeometry : public Geometry
{
public:
  VolumeGeometry();

  void Generate() override;
};
