#pragma once

#include <vector>

#include "Geometry.h"

class ImportedGeometry : public Geometry
{
public:
  ImportedGeometry(std::vector<Vertex> importedVertices,
                   std::vector<unsigned int> importedIndices);

  void Generate() override;

private:
  std::vector<Vertex> importedVertices;
  std::vector<unsigned int> importedIndices;
};
