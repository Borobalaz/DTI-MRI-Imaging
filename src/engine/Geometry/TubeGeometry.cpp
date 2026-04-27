#include "Geometry/TubeGeometry.h"

#include <glm/gtc/constants.hpp>

TubeGeometry::TubeGeometry(std::vector<Vertex> vertices,
                           std::vector<unsigned int> indices,
                           float radius,
                           unsigned int radialSegments)
{
  this->vertices = std::move(vertices);
  this->indices = std::move(indices);
  this->radius = radius;
  this->radialSegments = radialSegments;

  Generate();
  Upload();
}

void TubeGeometry::Generate()
{
  std::vector<Vertex> tubeVertices;
  std::vector<unsigned int> tubeIndices;

  if (indices.empty() || vertices.empty())
    return;

  const unsigned int RESTART = std::numeric_limits<unsigned int>::max();
  const unsigned int ringVertexCount = radialSegments;

  // helper: build local frame
  auto computeFrame = [](const glm::vec3& tangent)
  {
    glm::vec3 up = glm::abs(tangent.y) < 0.99f
                     ? glm::vec3(0,1,0)
                     : glm::vec3(1,0,0);

    glm::vec3 normal = glm::normalize(glm::cross(tangent, up));
    glm::vec3 binormal = glm::normalize(glm::cross(tangent, normal));

    return std::pair(normal, binormal);
  };

  size_t i = 0;

  while (i < indices.size())
  {
    // skip restart markers
    if (indices[i] == RESTART)
    {
      ++i;
      continue;
    }

    // collect one segment (one streamline)
    std::vector<unsigned int> segment;
    while (i < indices.size() && indices[i] != RESTART)
    {
      segment.push_back(indices[i]);
      ++i;
    }

    if (segment.size() < 2)
      continue;

    const unsigned int baseVertex = static_cast<unsigned int>(tubeVertices.size());

    // === build rings ===
    for (size_t s = 0; s < segment.size(); ++s)
    {
      const glm::vec3& p = vertices[segment[s]].position;

      // compute tangent
      glm::vec3 tangent;
      if (s < segment.size() - 1)
      {
        tangent = vertices[segment[s + 1]].position - p;
      }
      else
      {
        tangent = p - vertices[segment[s - 1]].position;
      }

      if (glm::length(tangent) > 1e-6f)
        tangent = glm::normalize(tangent);
      else
        tangent = glm::vec3(0,1,0);

      auto [normal, binormal] = computeFrame(tangent);

      // create ring
      for (unsigned int j = 0; j < radialSegments; ++j)
      {
        float angle = 2.0f * glm::pi<float>() * j / radialSegments;

        glm::vec3 offset =
          cos(angle) * normal * radius +
          sin(angle) * binormal * radius;

        Vertex v;
        v.position = p + offset;
        v.normal = glm::normalize(offset);
        v.texCoord = glm::vec2(
          (float)j / radialSegments,
          (float)s / segment.size()
        );

        tubeVertices.push_back(v);
      }
    }

    // === connect rings ===
    for (size_t s = 0; s < segment.size() - 1; ++s)
    {
      unsigned int ring0 = baseVertex + s * ringVertexCount;
      unsigned int ring1 = baseVertex + (s + 1) * ringVertexCount;

      for (unsigned int j = 0; j < radialSegments; ++j)
      {
        unsigned int next = (j + 1) % radialSegments;

        unsigned int a = ring0 + j;
        unsigned int b = ring0 + next;
        unsigned int c = ring1 + j;
        unsigned int d = ring1 + next;

        // triangle 1
        tubeIndices.push_back(a);
        tubeIndices.push_back(c);
        tubeIndices.push_back(b);

        // triangle 2
        tubeIndices.push_back(b);
        tubeIndices.push_back(c);
        tubeIndices.push_back(d);
      }
    }
  }

  // replace geometry
  vertices = std::move(tubeVertices);
  indices = std::move(tubeIndices);
}