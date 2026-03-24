#include "Preprocessing/stages/DatasetDiscoveryStage.h"

#include <memory>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Preprocessing/MriPreprocessingStages.h"

namespace
{
  bool EndsWith(const std::string& value, const std::string& suffix)
  {
    if (suffix.size() > value.size())
    {
      return false;
    }

    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin(),
      [](char a, char b)
      {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(a))) ==
               static_cast<char>(std::tolower(static_cast<unsigned char>(b)));
      });
  }

  std::string ToLower(std::string text)
  {
    std::transform(text.begin(), text.end(), text.begin(),
      [](char c)
      {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      });
    return text;
  }

  bool IsSupportedInputFile(const std::filesystem::path& path)
  {
    const std::string asString = ToLower(path.string());
    return EndsWith(asString, ".nii") ||
      EndsWith(asString, ".nii.gz") ||
      EndsWith(asString, ".nrrd") ||
      EndsWith(asString, ".mha") ||
      EndsWith(asString, ".mhd") ||
      EndsWith(asString, ".vxa");
  }

  bool MatchesSubjectSessionFilter(const std::string& path,
                                   const std::string& subjectId,
                                   const std::string& sessionId)
  {
    if (!subjectId.empty() && path.find(subjectId) == std::string::npos)
    {
      return false;
    }

    if (!sessionId.empty() && path.find(sessionId) == std::string::npos)
    {
      return false;
    }

    return true;
  }

  int ScoreCandidatePath(const std::string& path,
                         bool preferAnatomicalVolumes)
  {
    const std::string lowerPath = ToLower(path);
    int score = 0;

    if (preferAnatomicalVolumes)
    {
      if (lowerPath.find("/anat/") != std::string::npos || lowerPath.find("\\anat\\") != std::string::npos)
      {
        score += 300;
      }
      if (lowerPath.find("_t1w") != std::string::npos)
      {
        score += 450;
      }
    }

    if (lowerPath.find("/dwi/") != std::string::npos || lowerPath.find("\\dwi\\") != std::string::npos)
    {
      score += 500;
    }

    if (lowerPath.find("_bold") != std::string::npos)
    {
      score += 100;
    }

    return score;
  }

}

const char* DatasetDiscoveryStage::Name() const
{
  return "Dataset discovery";
}

void DatasetDiscoveryStage::Execute(MriPreprocessingContext& context) const
{
  namespace fs = std::filesystem;

  const fs::path root(context.request.datasetRootPath);
  if (!fs::exists(root) || !fs::is_directory(root))
  {
    throw std::invalid_argument("Dataset root path is missing or not a directory: " + context.request.datasetRootPath);
  }

  std::vector<std::pair<int, std::string>> scoredCandidates;

  for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root))
  {
    if (!entry.is_regular_file())
    {
      continue;
    }

    const fs::path filePath = entry.path();
    if (!IsSupportedInputFile(filePath))
    {
      continue;
    }

    const std::string absolutePath = filePath.string();
    if (!MatchesSubjectSessionFilter(
      absolutePath,
      context.request.preferredSubjectId,
      context.request.preferredSessionId))
    {
      continue;
    }

    scoredCandidates.emplace_back(
      ScoreCandidatePath(absolutePath, context.request.preferAnatomicalVolumes),
      absolutePath);
  }

  if (scoredCandidates.empty())
  {
    throw std::runtime_error("No supported MRI volume files found in dataset root.");
  }

  std::sort(scoredCandidates.begin(), scoredCandidates.end(),
    [](const auto& lhs, const auto& rhs)
    {
      if (lhs.first != rhs.first)
      {
        return lhs.first > rhs.first;
      }

      return lhs.second < rhs.second;
    });

  context.candidateVolumePaths.clear();
  context.candidateVolumePaths.reserve(scoredCandidates.size());
  for (const auto& scored : scoredCandidates)
  {
    context.candidateVolumePaths.push_back(scored.second);
  }

  context.selectedSourceVolumePath = context.candidateVolumePaths.front();
  context.report.sourceVolumePath = context.selectedSourceVolumePath;

  if (scoredCandidates.front().first < 300)
  {
    context.report.warnings.push_back(
      "Selected source volume is not an anatomical/T1-weighted file; results may be less stable.");
  }
}

std::unique_ptr<IMriPreprocessingStage> CreateDatasetDiscoveryStage()
{
  return std::make_unique<DatasetDiscoveryStage>();
}
