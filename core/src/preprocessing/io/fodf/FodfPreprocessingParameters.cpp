#include "Preprocessing/io/fodf/FodfPreprocessingParameters.h"

#include <nlohmann/json.hpp>

std::string BuildFodfPreprocessingParametersJson(
    const MriPreprocessingRequest& request,
    const MriFodfSettings& settings,
    float shellBValue,
    const std::vector<float>& ignoredShellBValues)
{
  nlohmann::json root;
  root["dwiVolumePath"] = request.dwiVolumePath;
  root["bvalPath"] = request.bvalPath;
  root["bvecPath"] = request.bvecPath;

  nlohmann::json fodf;
  fodf["lmax"] = settings.GetLmax();
  fodf["responseVoxelCount"] = settings.GetResponseVoxelCount();
  fodf["responseFaFloor"] = settings.GetResponseFaFloor();
  fodf["negativityThresholdTau"] = settings.GetNegativityThresholdTau();
  fodf["lambda"] = settings.GetLambda();
  fodf["maxIterations"] = settings.GetMaxIterations();
  fodf["shellTolerance"] = settings.GetShellTolerance();
  root["fodf"] = std::move(fodf);

  nlohmann::json shell;
  shell["bValue"] = shellBValue;
  shell["ignoredBValues"] = ignoredShellBValues;
  root["shell"] = std::move(shell);

  return root.dump(2);
}

void ApplyFodfPreprocessingParametersJson(
    const std::string& parametersJson,
    MriPreprocessingRequest& requestOut,
    MriFodfSettings& settingsOut)
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

  if (!root.contains("fodf"))
  {
    return;
  }

  const nlohmann::json& fodf = root.at("fodf");
  settingsOut.SetLmax(fodf.value("lmax", settingsOut.GetLmax()));
  settingsOut.SetResponseVoxelCount(fodf.value("responseVoxelCount", settingsOut.GetResponseVoxelCount()));
  settingsOut.SetResponseFaFloor(fodf.value("responseFaFloor", settingsOut.GetResponseFaFloor()));
  settingsOut.SetNegativityThresholdTau(fodf.value("negativityThresholdTau", settingsOut.GetNegativityThresholdTau()));
  settingsOut.SetLambda(fodf.value("lambda", settingsOut.GetLambda()));
  settingsOut.SetMaxIterations(fodf.value("maxIterations", settingsOut.GetMaxIterations()));
  settingsOut.SetShellTolerance(fodf.value("shellTolerance", settingsOut.GetShellTolerance()));
}

std::vector<float> ReadFodfIgnoredShellBValues(const std::string& parametersJson)
{
  std::vector<float> ignoredBValues;
  if (parametersJson.empty())
  {
    return ignoredBValues;
  }

  const nlohmann::json root = nlohmann::json::parse(parametersJson, nullptr, false);
  if (root.is_discarded() || !root.is_object() || !root.contains("shell"))
  {
    return ignoredBValues;
  }

  const nlohmann::json& shell = root.at("shell");
  if (!shell.is_object() || !shell.contains("ignoredBValues") || !shell.at("ignoredBValues").is_array())
  {
    return ignoredBValues;
  }

  for (const nlohmann::json& value : shell.at("ignoredBValues"))
  {
    if (value.is_number())
    {
      ignoredBValues.push_back(value.get<float>());
    }
  }
  return ignoredBValues;
}
