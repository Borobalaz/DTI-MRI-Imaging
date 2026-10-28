#pragma once

#include "render/geometry/Geometry.h"

class QuadGeometry : public Geometry
{
public:
	QuadGeometry();
	void Generate() override;
};
