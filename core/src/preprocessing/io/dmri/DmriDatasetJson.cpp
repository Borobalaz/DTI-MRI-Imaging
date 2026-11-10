#include "Preprocessing/io/dmri/DmriDatasetJson.h"

#include <stdexcept>

#include <nlohmann/json.hpp>

namespace
{
  constexpr int kSeedPointsFormatVersion = 1;
  constexpr int kFodfStatusFormatVersion = 1;

  nlohmann::json ParseOrThrow(const std::string &jsonText, const char *fileName)
  {
    nlohmann::json root = nlohmann::json::parse(jsonText, nullptr, false);
    if (root.is_discarded() || !root.is_object())
    {
      throw std::runtime_error(std::string(fileName) + " is not valid JSON");
    }
    return root;
  }

  void RequireFormatVersion(const nlohmann::json &root, int expectedVersion, const char *fileName)
  {
    const int formatVersion = root.at("formatVersion").get<int>();
    if (formatVersion != expectedVersion)
    {
      throw std::runtime_error("Unsupported " + std::string(fileName) + " formatVersion " +
                               std::to_string(formatVersion));
    }
  }
}

std::string SerializeSeedPointsJson(const std::vector<glm::vec3> &seedPoints)
{
  nlohmann::json points = nlohmann::json::array();
  for (const glm::vec3 &point : seedPoints)
  {
    points.push_back({point.x, point.y, point.z});
  }

  nlohmann::json root;
  root["formatVersion"] = kSeedPointsFormatVersion;
  // Same normalized object space as the streamline meshes' vertices.
  root["space"] = "object";
  root["points"] = std::move(points);
  return root.dump(2);
}

std::vector<glm::vec3> DeserializeSeedPointsJson(const std::string &jsonText)
{
  const nlohmann::json root = ParseOrThrow(jsonText, "seeds.json");

  std::vector<glm::vec3> seedPoints;
  try
  {
    RequireFormatVersion(root, kSeedPointsFormatVersion, "seeds.json");
    for (const nlohmann::json &point : root.at("points"))
    {
      const std::vector<float> components = point.get<std::vector<float>>();
      if (components.size() != 3U)
      {
        throw std::runtime_error("seeds.json holds a point without exactly 3 components");
      }
      seedPoints.emplace_back(components[0], components[1], components[2]);
    }
  }
  catch (const nlohmann::json::exception &exception)
  {
    throw std::runtime_error(std::string("Malformed seeds.json: ") + exception.what());
  }
  return seedPoints;
}

std::string SerializeDmriFodfStatusJson(const DmriFodfStatus &status)
{
  nlohmann::json root;
  root["formatVersion"] = kFodfStatusFormatVersion;
  root["available"] = status.available;
  root["unavailableReason"] = status.unavailableReason;
  root["absoluteAmplitudeCutoff"] = status.absoluteAmplitudeCutoff;
  return root.dump(2);
}

DmriFodfStatus DeserializeDmriFodfStatusJson(const std::string &jsonText)
{
  const nlohmann::json root = ParseOrThrow(jsonText, "status.json");

  DmriFodfStatus status;
  try
  {
    RequireFormatVersion(root, kFodfStatusFormatVersion, "status.json");
    status.available = root.at("available").get<bool>();
    status.unavailableReason = root.at("unavailableReason").get<std::string>();
    status.absoluteAmplitudeCutoff = root.at("absoluteAmplitudeCutoff").get<float>();
  }
  catch (const nlohmann::json::exception &exception)
  {
    throw std::runtime_error(std::string("Malformed status.json: ") + exception.what());
  }
  return status;
}
