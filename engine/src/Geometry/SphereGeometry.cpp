#include "Geometry/SphereGeometry.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>

#include <cmath>

SphereGeometry::SphereGeometry(float radius_, unsigned int sectors_, unsigned int stacks_)
  : radius(radius_), sectors(sectors_), stacks(stacks_)
{
  Generate();
  Upload();
}

SphereGeometry::~SphereGeometry()
{
}

void SphereGeometry::Generate()
{
  vertices.clear();
  indices.clear();

  const float PI = glm::pi<float>();
  unsigned int sectorCount = sectors;
  unsigned int stackCount = stacks;

  // generate vertices
  for (unsigned int i = 0; i <= stackCount; ++i)
  {
    float stackAngle = PI / 2 - (float)i * PI / (float)stackCount; // from pi/2 to -pi/2
    float xy = radius * std::cos(stackAngle); // r * cos(u)
    float y = radius * std::sin(stackAngle);  // r * sin(u)

    for (unsigned int j = 0; j <= sectorCount; ++j)
    {
      float sectorAngle = (float)j * 2.0f * PI / (float)sectorCount; // 0 to 2pi

      Vertex v;
      float x = xy * std::cos(sectorAngle);
      float z = xy * std::sin(sectorAngle);
      v.position = glm::vec3(x, y, z);
      v.normal = glm::normalize(v.position);
      v.texCoord = glm::vec2((float)j / (float)sectorCount, (float)i / (float)stackCount);
      v.tangent = glm::vec3(0.0f);
      vertices.push_back(v);
    }
  }

  // generate indices
  for (unsigned int i = 0; i < stackCount; ++i)
  {
    unsigned int k1 = i * (sectorCount + 1);
    unsigned int k2 = k1 + sectorCount + 1;

    for (unsigned int j = 0; j < sectorCount; ++j, ++k1, ++k2)
    {
      if (i != 0)
      {
        indices.push_back(k1);
        indices.push_back(k2);
        indices.push_back(k1 + 1);
      }

      if (i != (stackCount - 1))
      {
        indices.push_back(k1 + 1);
        indices.push_back(k2);
        indices.push_back(k2 + 1);
      }
    }
  }

  // compute tangents per-triangle and accumulate per-vertex
  std::vector<glm::vec3> tanAccum(vertices.size(), glm::vec3(0.0f));

  for (size_t i = 0; i + 2 < indices.size(); i += 3)
  {
    unsigned int i0 = indices[i + 0];
    unsigned int i1 = indices[i + 1];
    unsigned int i2 = indices[i + 2];

    const glm::vec3& p0 = vertices[i0].position;
    const glm::vec3& p1 = vertices[i1].position;
    const glm::vec3& p2 = vertices[i2].position;

    const glm::vec2& uv0 = vertices[i0].texCoord;
    const glm::vec2& uv1 = vertices[i1].texCoord;
    const glm::vec2& uv2 = vertices[i2].texCoord;

    glm::vec3 edge1 = p1 - p0;
    glm::vec3 edge2 = p2 - p0;
    glm::vec2 deltaUV1 = uv1 - uv0;
    glm::vec2 deltaUV2 = uv2 - uv0;

    float denom = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
    if (glm::epsilonEqual(denom, 0.0f, 1e-6f))
      continue;

    float f = 1.0f / denom;

    glm::vec3 tangent = f * (edge1 * deltaUV2.y - edge2 * deltaUV1.y);

    tanAccum[i0] += tangent;
    tanAccum[i1] += tangent;
    tanAccum[i2] += tangent;
  }

  for (size_t i = 0; i < vertices.size(); ++i)
  {
    glm::vec3 t = tanAccum[i];
    if (glm::length(t) > 1e-6f)
      vertices[i].tangent = glm::normalize(t);
    else
      vertices[i].tangent = glm::vec3(1.0f, 0.0f, 0.0f);
  }
}
