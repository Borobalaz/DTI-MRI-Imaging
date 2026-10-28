#include "Geometry/IcosphereGeometry.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/glm.hpp>

#include <cmath>
#include <map>
#include <utility>

IcosphereGeometry::IcosphereGeometry(unsigned int subdivisionLevel_)
  : subdivisionLevel(subdivisionLevel_)
{
  Generate();
}

IcosphereGeometry::~IcosphereGeometry()
{
}

void IcosphereGeometry::Generate()
{
  vertices.clear();
  indices.clear();

  std::vector<glm::vec3> positions;
  BuildIcosphere(subdivisionLevel, positions, indices);

  const float PI = glm::pi<float>();
  vertices.reserve(positions.size());
  for (const glm::vec3& position : positions)
  {
    Vertex v;
    v.position = position;
    v.normal = position;
    // Spherical UV (y up, like SphereGeometry); it has a seam, which is fine since nothing samples it.
    v.texCoord = glm::vec2(0.5f + std::atan2(position.z, position.x) / (2.0f * PI),
                           std::acos(glm::clamp(position.y, -1.0f, 1.0f)) / PI);
    v.tangent = glm::vec3(0.0f);
    vertices.push_back(v);
  }

  ComputeTangents();
}

std::vector<glm::vec3> IcosphereGeometry::GetDirections() const
{
  std::vector<glm::vec3> directions;
  directions.reserve(vertices.size());
  for (const Vertex& vertex : vertices)
  {
    directions.push_back(vertex.position);
  }
  return directions;
}

std::vector<glm::vec3> IcosphereGeometry::GenerateDirections(unsigned int level)
{
  std::vector<glm::vec3> positions;
  std::vector<unsigned int> triangles;
  BuildIcosphere(level, positions, triangles);
  return positions;
}

void IcosphereGeometry::BuildIcosphere(unsigned int level, std::vector<glm::vec3>& positions, std::vector<unsigned int>& triangles)
{
  const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

  positions = {
    {-1.0f, t, 0.0f}, {1.0f, t, 0.0f}, {-1.0f, -t, 0.0f}, {1.0f, -t, 0.0f},
    {0.0f, -1.0f, t}, {0.0f, 1.0f, t}, {0.0f, -1.0f, -t}, {0.0f, 1.0f, -t},
    {t, 0.0f, -1.0f}, {t, 0.0f, 1.0f}, {-t, 0.0f, -1.0f}, {-t, 0.0f, 1.0f}
  };
  for (glm::vec3& position : positions)
  {
    position = glm::normalize(position);
  }

  triangles = {
    0, 11, 5,   0, 5, 1,    0, 1, 7,    0, 7, 10,   0, 10, 11,
    1, 5, 9,    5, 11, 4,   11, 10, 2,  10, 7, 6,   7, 1, 8,
    3, 9, 4,    3, 4, 2,    3, 2, 6,    3, 6, 8,    3, 8, 9,
    4, 9, 5,    2, 4, 11,   6, 2, 10,   8, 6, 7,    9, 8, 1
  };

  for (unsigned int pass = 0; pass < level; ++pass)
  {
    // Edge (lower index, higher index) -> midpoint vertex, so neighbouring triangles share it.
    std::map<std::pair<unsigned int, unsigned int>, unsigned int> midpointCache;
    auto midpoint = [&](unsigned int a, unsigned int b) -> unsigned int
    {
      const std::pair<unsigned int, unsigned int> edge = a < b ? std::make_pair(a, b) : std::make_pair(b, a);
      const auto cached = midpointCache.find(edge);
      if (cached != midpointCache.end())
      {
        return cached->second;
      }

      const unsigned int index = static_cast<unsigned int>(positions.size());
      positions.push_back(glm::normalize(positions[a] + positions[b]));
      midpointCache.emplace(edge, index);
      return index;
    };

    std::vector<unsigned int> subdivided;
    subdivided.reserve(triangles.size() * 4);
    for (size_t i = 0; i + 2 < triangles.size(); i += 3)
    {
      const unsigned int v0 = triangles[i + 0];
      const unsigned int v1 = triangles[i + 1];
      const unsigned int v2 = triangles[i + 2];
      const unsigned int m01 = midpoint(v0, v1);
      const unsigned int m12 = midpoint(v1, v2);
      const unsigned int m20 = midpoint(v2, v0);

      subdivided.insert(subdivided.end(), {v0, m01, m20});
      subdivided.insert(subdivided.end(), {v1, m12, m01});
      subdivided.insert(subdivided.end(), {v2, m20, m12});
      subdivided.insert(subdivided.end(), {m01, m12, m20});
    }
    triangles = std::move(subdivided);
  }
}

// Same per-triangle accumulation (and fallback) as SphereGeometry.
void IcosphereGeometry::ComputeTangents()
{
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
