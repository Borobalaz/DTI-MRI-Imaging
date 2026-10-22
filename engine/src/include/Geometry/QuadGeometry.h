#pragma once

#include "Render/Geometry/Geometry.h"

class QuadGeometry : public Geometry
{
public:
	QuadGeometry();
	void Generate() override;
};
