#include "Preprocessing/stages/DwiGradientValidationStage.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Preprocessing/MriPreprocessingStages.h"

namespace
{
  std::vector<std::vector<float>> ParseLineWiseFloatTable(const std::string& path)
  {
    std::ifstream input(path);
    if (!input.is_open())
    {
      throw std::runtime_error("Failed to open gradient sidecar file: " + path);
    }

    std::vector<std::vector<float>> rows;
    std::string line;
    while (std::getline(input, line))
    {
      std::istringstream lineStream(line);
      std::vector<float> row;
      float value = 0.0f;
      while (lineStream >> value)
      {
        row.push_back(value);
      }

      if (!row.empty())
      {
        rows.push_back(std::move(row));
      }
    }

    return rows;
  }

  std::vector<float> ParseBvals(const std::string& path)
  {
    const std::vector<std::vector<float>> rows = ParseLineWiseFloatTable(path);
    std::vector<float> values;
    for (const std::vector<float>& row : rows)
    {
      values.insert(values.end(), row.begin(), row.end());
    }
    return values;
  }

  std::vector<std::vector<float>> ParseBvecRows(const std::string& path)
  {
    const std::vector<std::vector<float>> rows = ParseLineWiseFloatTable(path);

    if (rows.empty())
    {
      return {};
    }

    if (rows.size() == 3)
    {
      return rows;
    }

    const bool looksLikeNx3 = std::all_of(rows.begin(), rows.end(),
      [](const std::vector<float>& row)
      {
        return row.size() == 3;
      });

    if (!looksLikeNx3)
    {
      throw std::runtime_error("bvec file must be either 3xN (three rows) or Nx3 (three columns).");
    }

    std::vector<std::vector<float>> rowMajor(3);
    rowMajor[0].reserve(rows.size());
    rowMajor[1].reserve(rows.size());
    rowMajor[2].reserve(rows.size());
    for (const std::vector<float>& row : rows)
    {
      rowMajor[0].push_back(row[0]);
      rowMajor[1].push_back(row[1]);
      rowMajor[2].push_back(row[2]);
    }

    return rowMajor;
  }

  bool IsFiniteFloat(float value)
  {
    return std::isfinite(value) != 0;
  }

  std::filesystem::path ResolveRequestPath(const std::string& requestedPath)
  {
    namespace fs = std::filesystem;
    const fs::path candidate(requestedPath);
    if (candidate.is_absolute())
    {
      return candidate;
    }

    if (fs::exists(candidate))
    {
      return candidate;
    }

    return candidate;
  }
}

/**
 * @brief Get the name of the preprocessing stage.
 * 
 * @return const char* 
 */
const char* DWIGradientNormalizationStage::Name() const
{
  return "DWI gradient validation";
}

/**
 * @brief 
 * 
 * @param context 
 */
void DWIGradientNormalizationStage::Execute(MriPreprocessingContext& context) const
{
  const std::filesystem::path dwiPath =
    ResolveRequestPath(context.request.dwiVolumePath);
  const std::filesystem::path bvalPath =
    ResolveRequestPath(context.request.bvalPath);
  const std::filesystem::path bvecPath =
    ResolveRequestPath(context.request.bvecPath);

  // Populate context with resolved paths and parsed metadata for downstream stages
  context.selectedDwiVolumePath = dwiPath.string();
  context.selectedBvalPath = bvalPath.string();
  context.selectedBvecPath = bvecPath.string();
  context.report.sourceVolumePath = context.selectedDwiVolumePath;

  // Parse bvals and bvecs
  context.bValues = ParseBvals(context.selectedBvalPath);
  const std::vector<std::vector<float>> bvecRows = ParseBvecRows(context.selectedBvecPath);

  if (context.bValues.empty())
  {
    throw std::runtime_error("bval file does not contain any values: " + context.selectedBvalPath);
  }

  if (bvecRows.size() != 3)
  {
    throw std::runtime_error("bvec parsing failed: expected exactly 3 rows after normalization.");
  }

  const size_t directionCount = context.bValues.size();
  if (bvecRows[0].size() != directionCount ||
      bvecRows[1].size() != directionCount ||
      bvecRows[2].size() != directionCount)
  {
    throw std::runtime_error(
      "Mismatch between bval count and bvec direction count. bval=" +
      std::to_string(directionCount) +
      " bvec=" + std::to_string(bvecRows[0].size()));
  }

  context.gradientDirections.clear();
  context.gradientDirections.reserve(directionCount);

  // Normalize gradient directions
  size_t nonB0Count = 0;
  for (size_t i = 0; i < directionCount; ++i)
  {
    const float b = context.bValues[i];
    if (!IsFiniteFloat(b) || b < 0.0f)
    {
      throw std::runtime_error("Invalid b-value at index " + std::to_string(i) + ".");
    }

    const float gx = bvecRows[0][i];
    const float gy = bvecRows[1][i];
    const float gz = bvecRows[2][i];
    if (!IsFiniteFloat(gx) || !IsFiniteFloat(gy) || !IsFiniteFloat(gz))
    {
      throw std::runtime_error("Invalid b-vector component at index " + std::to_string(i) + ".");
    }

    glm::vec3 g(gx, gy, gz);
    const float length = glm::length(g);

    if (b > 50.0f)
    {
      ++nonB0Count;
      if (length < std::numeric_limits<float>::epsilon())
      {
        throw std::runtime_error("Non-b0 direction has zero gradient vector at index " + std::to_string(i) + ".");
      }

      if (std::fabs(length - 1.0f) > 0.15f)
      {
        context.report.warnings.push_back(
          "Gradient direction norm deviates from unit length at index " + std::to_string(i) +
          "; normalizing in-memory before downstream use.");
      }

      g /= length;
    }
    else
    {
      if (length > 0.25f)
      {
        context.report.warnings.push_back(
          "b0 volume has non-trivial gradient vector at index " + std::to_string(i) + ".");
      }
    }

    context.gradientDirections.push_back(g);
  }

  if (nonB0Count < 6)
  {
    context.report.warnings.push_back(
      "DWI appears to have very few diffusion-weighted directions (< 6); tensor fitting may be unstable.");
  }

  context.gradientMetadataValid = true;
}

/**
 * @brief Create a Dwi Gradient Validation Stage object
 * 
 * @return std::unique_ptr<IMriPreprocessingStage> 
 */
std::unique_ptr<IMriPreprocessingStage> CreateDwiGradientNormalizationStage()
{
  return std::make_unique<DWIGradientNormalizationStage>();
}
