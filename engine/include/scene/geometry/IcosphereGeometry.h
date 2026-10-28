#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "render/geometry/Geometry.h"

// Unit-radius icosphere: a subdivided icosahedron with shared vertices (10 * 4^level + 2 of them,
// 642 at level 3). The vertex order is deterministic for a given level, so per-vertex tables
// indexed by gl_VertexID (e.g. the SH glyph basis table) line up with GetDirections().
class IcosphereGeometry : public Geometry
{
public:
  explicit IcosphereGeometry(unsigned int subdivisionLevel = 3);
  ~IcosphereGeometry() override;

  void Generate() override;

  unsigned int GetSubdivisionLevel() const { return subdivisionLevel; }

  // Unit vertex positions in vertex (gl_VertexID) order.
  std::vector<glm::vec3> GetDirections() const;

  // The same direction set GetDirections() returns for `level`, without building a Geometry.
  static std::vector<glm::vec3> GenerateDirections(unsigned int level);

private:
  // Unit positions and CCW-outward triangles of the icosahedron subdivided `level` times.
  static void BuildIcosphere(unsigned int level, std::vector<glm::vec3>& positions, std::vector<unsigned int>& triangles);

  void ComputeTangents();

  unsigned int subdivisionLevel;
};
