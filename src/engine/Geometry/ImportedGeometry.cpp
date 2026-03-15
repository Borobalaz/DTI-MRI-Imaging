#include "ImportedGeometry.h"

#include <algorithm>

namespace
{
  bool HasUsefulNormals(const std::vector<Vertex>& vertices)
  {
    return std::any_of(
      vertices.begin(),
      vertices.end(),
      [](const Vertex& vertex)
      {
        return glm::length(vertex.normal) > 0.0001f;
      }
    );
  }
}

ImportedGeometry::ImportedGeometry(std::vector<Vertex> importedVertices,
                                   std::vector<unsigned int> importedIndices)
  : importedVertices(std::move(importedVertices)),
    importedIndices(std::move(importedIndices))
{
  Generate();
  Upload();
}

void ImportedGeometry::Generate()
{
  vertices = importedVertices;
  indices = importedIndices;

  if (!HasUsefulNormals(vertices))
  {
    ComputeNormals();
  }
}
