#include "Preprocessing/io/dmri/DmriPreprocessingParameters.h"

#include <nlohmann/json.hpp>

#include "Preprocessing/io/dti/DtiPreprocessingParameters.h"
#include "Preprocessing/io/fodf/FodfPreprocessingParameters.h"

namespace
{
  constexpr const char *kFodfTractographyKey = "fodfTractography";
}

std::string BuildDmriPreprocessingParametersJson(
    const MriPreprocessingRequest &request,
    const MriTractographySettings &tractographySettings,
    const MriFodfSettings &fodfSettings,
    const MriFodfTractographySettings &fodfTractographySettings,
    const MriPreprocessingResult &result)
{
  // Composed from the legacy builders so the shared blocks keep exactly their legacy shape, which
  // is what lets the DTI/fODF Apply functions read a dmri JSON unchanged.
  nlohmann::json root = nlohmann::json::parse(BuildDtiPreprocessingParametersJson(
      request, tractographySettings, tractographySettings.GetMatchedPresetName()));
  const nlohmann::json fodfRoot = nlohmann::json::parse(BuildFodfPreprocessingParametersJson(
      request, fodfSettings, result.fodfResponse.bValue, result.fodfIgnoredShellBValues));
  root["fodf"] = fodfRoot.at("fodf");
  root["shell"] = fodfRoot.at("shell");

  nlohmann::json fodfTractography;
  fodfTractography["relativeAmplitudeCutoff"] = fodfTractographySettings.GetRelativeAmplitudeCutoff();
  fodfTractography["maxAngleDegrees"] = fodfTractographySettings.GetMaxAngleDegrees();
  fodfTractography["absoluteAmplitudeCutoff"] = result.fodfAbsoluteAmplitudeCutoff; // provenance only
  root[kFodfTractographyKey] = std::move(fodfTractography);

  return root.dump(2);
}

void ApplyDmriPreprocessingParametersJson(
    const std::string &parametersJson,
    MriPreprocessingRequest &requestOut,
    MriTractographySettings &tractographySettingsOut,
    MriFodfSettings &fodfSettingsOut,
    MriFodfTractographySettings &fodfTractographySettingsOut)
{
  ApplyDtiPreprocessingParametersJson(parametersJson, requestOut, tractographySettingsOut);
  ApplyFodfPreprocessingParametersJson(parametersJson, requestOut, fodfSettingsOut);

  if (parametersJson.empty())
  {
    return;
  }

  const nlohmann::json root = nlohmann::json::parse(parametersJson, nullptr, false);
  if (root.is_discarded() || !root.is_object() || !root.contains(kFodfTractographyKey) ||
      !root.at(kFodfTractographyKey).is_object())
  {
    return;
  }

  const nlohmann::json &fodfTractography = root.at(kFodfTractographyKey);
  fodfTractographySettingsOut.SetRelativeAmplitudeCutoff(fodfTractography.value(
      "relativeAmplitudeCutoff", fodfTractographySettingsOut.GetRelativeAmplitudeCutoff()));
  fodfTractographySettingsOut.SetMaxAngleDegrees(fodfTractography.value(
      "maxAngleDegrees", fodfTractographySettingsOut.GetMaxAngleDegrees()));
}
