#include "Preprocessing/io/dti/DtiPreprocessingParameters.h"

#include <cstdint>

#include <nlohmann/json.hpp>

std::string BuildDtiPreprocessingParametersJson(
    const MriPreprocessingRequest &request,
    const MriTractographySettings &settings,
    const std::string &matchedPresetName)
{
  nlohmann::json root;
  root["dwiVolumePath"] = request.dwiVolumePath;
  root["bvalPath"] = request.bvalPath;
  root["bvecPath"] = request.bvecPath;

  nlohmann::json tractography;
  tractography["preset"] = matchedPresetName;
  tractography["faSeedThreshold"] = settings.GetFaSeedThreshold();
  tractography["faStopThreshold"] = settings.GetFaStopThreshold();
  tractography["l1StopThreshold"] = settings.GetL1StopThreshold();
  tractography["stepSizeVoxels"] = settings.GetStepSizeVoxels();
  tractography["maxStepsPerStreamline"] = settings.GetMaxStepsPerStreamline();
  tractography["seedStride"] = settings.GetSeedStride();
  tractography["maxSeeds"] = static_cast<uint64_t>(settings.GetMaxSeeds());
  tractography["minPointsPerStreamline"] = static_cast<uint64_t>(settings.GetMinPointsPerStreamline());
  tractography["tubeRadius"] = settings.GetTubeRadius();
  tractography["tubeRadialSegments"] = settings.GetTubeRadialSegments();
  root["tractography"] = std::move(tractography);

  return root.dump(2);
}

void ApplyDtiPreprocessingParametersJson(
    const std::string &parametersJson,
    MriPreprocessingRequest &requestOut,
    MriTractographySettings &settingsOut)
{
  if (parametersJson.empty())
  {
    return;
  }

  const nlohmann::json root = nlohmann::json::parse(parametersJson, nullptr, false);
  if (root.is_discarded())
  {
    return;
  }

  requestOut.dwiVolumePath = root.value("dwiVolumePath", requestOut.dwiVolumePath);
  requestOut.bvalPath = root.value("bvalPath", requestOut.bvalPath);
  requestOut.bvecPath = root.value("bvecPath", requestOut.bvecPath);

  if (!root.contains("tractography"))
  {
    return;
  }

  const nlohmann::json &tractography = root.at("tractography");
  settingsOut.SetFaSeedThreshold(tractography.value("faSeedThreshold", settingsOut.GetFaSeedThreshold()));
  settingsOut.SetFaStopThreshold(tractography.value("faStopThreshold", settingsOut.GetFaStopThreshold()));
  settingsOut.SetL1StopThreshold(tractography.value("l1StopThreshold", settingsOut.GetL1StopThreshold()));
  settingsOut.SetStepSizeVoxels(tractography.value("stepSizeVoxels", settingsOut.GetStepSizeVoxels()));
  settingsOut.SetMaxStepsPerStreamline(
      tractography.value("maxStepsPerStreamline", settingsOut.GetMaxStepsPerStreamline()));
  settingsOut.SetSeedStride(tractography.value("seedStride", settingsOut.GetSeedStride()));
  settingsOut.SetMaxSeeds(static_cast<size_t>(
      tractography.value("maxSeeds", static_cast<uint64_t>(settingsOut.GetMaxSeeds()))));
  settingsOut.SetMinPointsPerStreamline(static_cast<size_t>(
      tractography.value("minPointsPerStreamline", static_cast<uint64_t>(settingsOut.GetMinPointsPerStreamline()))));
  settingsOut.SetTubeRadius(tractography.value("tubeRadius", settingsOut.GetTubeRadius()));
  settingsOut.SetTubeRadialSegments(
      tractography.value("tubeRadialSegments", settingsOut.GetTubeRadialSegments()));
}
