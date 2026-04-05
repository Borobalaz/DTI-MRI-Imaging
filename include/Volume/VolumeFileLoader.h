#pragma once

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "VolumeData.h"
#include "VolumeSeriesData.h"

struct VolumeFileHeader
{
  char magic[4] = {'V', 'X', 'A', '1'};
  uint32_t version = 1;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 0;
  float spacingX = 1.0f;
  float spacingY = 1.0f;
  float spacingZ = 1.0f;
};

class VolumeFileLoader
{
public:
  static std::optional<VolumeData> Load(const std::string &filePath);
  static std::optional<VolumeSeriesData> LoadSeries(const std::string &filePath);
  static std::optional<VolumeData> LoadFrame(const std::string &filePath, int frameIndex);
  static std::string GetLastError();
  static std::optional<VolumeData> LoadTyped(const std::string &filePath);
  static bool Save(const std::string &filePath, const VolumeData &volume);

protected:
  static std::optional<VolumeData> LoadTypedVxa(const std::string &filePath);
  static std::optional<VolumeData> TryLoadMedicalFormat(const std::string &filePath);
  static std::optional<VolumeSeriesData> TryLoadMedicalSeriesFormat(const std::string &filePath);
};