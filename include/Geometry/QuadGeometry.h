#pragma once

#include "Geometry.h"

class QuadGeometry : public Geometry
{
public:
	QuadGeometry();
	void Generate() override;
};
