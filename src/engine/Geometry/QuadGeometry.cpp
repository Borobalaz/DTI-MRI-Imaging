#include "QuadGeometry.h"

QuadGeometry::QuadGeometry() 
{
  this->Generate();
  this->Upload();
}

void QuadGeometry::Generate()
{
	vertices =
	{
		{ glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f) },
		{ glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f) },
		{ glm::vec3( 0.5f,  0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f) },
		{ glm::vec3(-0.5f,  0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f) }
	};

	indices =
	{
		0, 1, 2,
		2, 3, 0
	};
}
