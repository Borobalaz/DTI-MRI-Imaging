#pragma once

#include "Geometry/Geometry.h"

class QuadGeometry : public Geometry
{
public:
	QuadGeometry();
	void Generate() override;
};
