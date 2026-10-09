#pragma once

#include "Geometry/Geometry.h"

// A single oversized NDC-space triangle covering the whole viewport, used to
// draw fullscreen postprocessing effects. No MVP transform -- the paired
// vertex shader passes positions straight through as clip-space coordinates.
class FullscreenTriangleGeometry : public Geometry
{
public:
  FullscreenTriangleGeometry();
  void Generate() override;
};
