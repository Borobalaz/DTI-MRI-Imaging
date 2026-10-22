#include "Geometry/StreamlineGeometry.h"

#include <utility>

StreamlineGeometry::StreamlineGeometry(std::vector<Vertex> vertices,
                                       std::vector<unsigned int> indices)
{
  this->vertices = std::move(vertices);
  this->indices = std::move(indices);
  SetPrimitive(PrimitiveTopology::LineStrip);
  SetPrimitiveRestartIndex(kRestartIndex);
  SetLineWidth(1.5f);
}

void StreamlineGeometry::Generate()
{
}
