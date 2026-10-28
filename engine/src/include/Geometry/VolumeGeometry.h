#pragma once

#include "render/geometry/Geometry.h"

class VolumeGeometry : public Geometry
{
public:
  VolumeGeometry();

  void Generate() override;
};
