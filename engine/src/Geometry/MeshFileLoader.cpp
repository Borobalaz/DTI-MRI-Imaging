#include "Geometry/MeshFileLoader.h"

#include <cstring>
#include <filesystem>
#include <fstream>

#include "Geometry/ImportedGeometry.h"

namespace
{
  std::string g_lastMeshLoaderError;

  void SetMeshLoaderError(const std::string &message)
  {
    g_lastMeshLoaderError = message;
  }
}

/**
 * @brief Write a Geometry's vertex/index buffers into a MSH1 file. Mirrors VolumeFileLoader's
 *        header-then-raw-buffer convention.
 */
bool MeshFileLoader::Save(const std::string &filePath, const Geometry &geometry)
{
  const std::filesystem::path path(filePath);
  if (path.has_parent_path())
  {
    std::filesystem::create_directories(path.parent_path());
  }

  std::ofstream output(filePath, std::ios::binary);
  if (!output.is_open())
  {
    SetMeshLoaderError("Failed to open mesh file for writing: " + filePath);
    return false;
  }

  const std::vector<Vertex> &vertices = geometry.GetVertices();
  const std::vector<unsigned int> &indices = geometry.GetIndices();

  MeshFileHeader header{};
  header.vertexCount = static_cast<uint32_t>(vertices.size());
  header.indexCount = static_cast<uint32_t>(indices.size());

  output.write(reinterpret_cast<const char *>(&header), sizeof(header));
  output.write(reinterpret_cast<const char *>(vertices.data()),
               static_cast<std::streamsize>(vertices.size() * sizeof(Vertex)));
  output.write(reinterpret_cast<const char *>(indices.data()),
               static_cast<std::streamsize>(indices.size() * sizeof(unsigned int)));

  return output.good();
}

/**
 * @brief Read a MSH1 file back into an ImportedGeometry (which already knows how to recompute
 *        normals and upload to the GPU from raw vertex/index arrays).
 */
std::shared_ptr<Geometry> MeshFileLoader::Load(const std::string &filePath)
{
  g_lastMeshLoaderError.clear();

  std::ifstream input(filePath, std::ios::binary);
  if (!input.is_open())
  {
    SetMeshLoaderError("Failed to open mesh file for reading: " + filePath);
    return nullptr;
  }

  MeshFileHeader header{};
  input.read(reinterpret_cast<char *>(&header), sizeof(header));
  if (!input || std::memcmp(header.magic, "MSH1", 4) != 0 || header.version != 1)
  {
    SetMeshLoaderError("Invalid or unsupported mesh file: " + filePath);
    return nullptr;
  }

  std::error_code sizeErrorCode;
  const uintmax_t actualSize = std::filesystem::file_size(filePath, sizeErrorCode);
  const uintmax_t expectedSize = sizeof(MeshFileHeader) +
      static_cast<uintmax_t>(header.vertexCount) * sizeof(Vertex) +
      static_cast<uintmax_t>(header.indexCount) * sizeof(unsigned int);
  if (sizeErrorCode || actualSize != expectedSize)
  {
    SetMeshLoaderError("Mesh file size does not match its header: " + filePath);
    return nullptr;
  }

  std::vector<Vertex> vertices(header.vertexCount);
  std::vector<unsigned int> indices(header.indexCount);

  input.read(reinterpret_cast<char *>(vertices.data()),
             static_cast<std::streamsize>(vertices.size() * sizeof(Vertex)));
  input.read(reinterpret_cast<char *>(indices.data()),
             static_cast<std::streamsize>(indices.size() * sizeof(unsigned int)));

  if (!input)
  {
    SetMeshLoaderError("Failed to read mesh data: " + filePath);
    return nullptr;
  }

  return std::make_shared<ImportedGeometry>(std::move(vertices), std::move(indices));
}

std::string MeshFileLoader::GetLastError()
{
  return g_lastMeshLoaderError;
}
