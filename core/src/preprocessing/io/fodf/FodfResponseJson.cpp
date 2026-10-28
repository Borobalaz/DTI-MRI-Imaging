#include "Preprocessing/io/fodf/FodfResponseJson.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

namespace
{
  constexpr int kResponseFormatVersion = 1;
}

std::string SerializeFodfResponseJson(const FodfResponseFunction& response)
{
  nlohmann::json root;
  root["formatVersion"] = kResponseFormatVersion;
  root["bValue"] = response.bValue;
  root["lmax"] = response.lmax;
  root["zonalCoefficients"] = response.zonalCoefficients;
  root["rotationalCoefficients"] = response.rotationalCoefficients;
  root["voxelCount"] = static_cast<std::uint64_t>(response.voxelCount);
  root["meanFa"] = response.meanFa;
  return root.dump(2);
}

FodfResponseFunction DeserializeFodfResponseJson(const std::string& jsonText)
{
  const nlohmann::json root = nlohmann::json::parse(jsonText, nullptr, false);
  if (root.is_discarded())
  {
    throw std::runtime_error("response.json is not valid JSON");
  }

  FodfResponseFunction response;
  try
  {
    const int formatVersion = root.at("formatVersion").get<int>();
    if (formatVersion != kResponseFormatVersion)
    {
      throw std::runtime_error("Unsupported response.json formatVersion " + std::to_string(formatVersion));
    }
    response.bValue = root.at("bValue").get<float>();
    response.lmax = root.at("lmax").get<int>();
    response.zonalCoefficients = root.at("zonalCoefficients").get<std::vector<float>>();
    response.rotationalCoefficients = root.at("rotationalCoefficients").get<std::vector<float>>();
    response.voxelCount = static_cast<size_t>(root.at("voxelCount").get<std::uint64_t>());
    response.meanFa = root.at("meanFa").get<float>();
  }
  catch (const nlohmann::json::exception& exception)
  {
    throw std::runtime_error(std::string("Malformed response.json: ") + exception.what());
  }

  if (!response.IsValid())
  {
    throw std::runtime_error("response.json does not describe a valid response function");
  }
  return response;
}
