#include "Preprocessing/MriTractographySettings.h"

#include <algorithm>
#include <cmath>

#include "ui/widgets/inspect_fields/InspectDropdownFieldWidget.h"
#include "ui/widgets/inspect_fields/InspectNumberFieldWidget.h"

namespace
{
struct PresetValues
{
  const char *displayName;
  float faSeedThreshold;
  float faStopThreshold;
  float l1StopThreshold;
  float stepSizeVoxels;
  int maxStepsPerStreamline;
  int seedStride;
  size_t maxSeeds;
  size_t minPointsPerStreamline;
  float tubeRadius;
  unsigned int tubeRadialSegments;
};

const std::vector<PresetValues> &GetPresetValues()
{
  static const std::vector<PresetValues> presets = {
      {
          "Balanced",
          0.4f,
          0.3f,
          1e-6f,
          0.3f,
          250,
          1,
          3000,
          15,
          0.001f,
          3U,
      },
      {
          "Fast Preview",
          0.5f,
          0.4f,
          1e-6f,
          0.45f,
          150,
          2,
          1500,
          10,
          0.0012f,
          3U,
      },
      {
          "High Sensitivity",
          0.3f,
          0.2f,
          1e-6f,
          0.2f,
          400,
          1,
          6000,
          20,
          0.0008f,
          4U,
      },
      {
        "Human Full Brain",
          0.6f,
          0.15f,
          1e-6f,
          0.65f,
          150,
          1,
          6000,
          30,
          0.001f,
          3U,
      },
  };

  return presets;
}

QStringList GetPresetOptions()
{
  QStringList options;
  options << "Custom";
  for (const PresetValues &preset : GetPresetValues())
  {
    options << preset.displayName;
  }
  return options;
}

bool ApproximatelyEqual(float lhs, float rhs)
{
  return std::abs(lhs - rhs) <= 1e-6f;
}

bool MatchesPreset(const MriTractographySettings &settings, const PresetValues &preset)
{
  return ApproximatelyEqual(settings.GetFaSeedThreshold(), preset.faSeedThreshold) &&
         ApproximatelyEqual(settings.GetFaStopThreshold(), preset.faStopThreshold) &&
         ApproximatelyEqual(settings.GetL1StopThreshold(), preset.l1StopThreshold) &&
         ApproximatelyEqual(settings.GetStepSizeVoxels(), preset.stepSizeVoxels) &&
         settings.GetMaxStepsPerStreamline() == preset.maxStepsPerStreamline &&
         settings.GetSeedStride() == preset.seedStride &&
         settings.GetMaxSeeds() == preset.maxSeeds &&
         settings.GetMinPointsPerStreamline() == preset.minPointsPerStreamline &&
         ApproximatelyEqual(settings.GetTubeRadius(), preset.tubeRadius) &&
         settings.GetTubeRadialSegments() == preset.tubeRadialSegments;
}

QString CurrentPresetName(const MriTractographySettings &settings)
{
  for (const PresetValues &preset : GetPresetValues())
  {
    if (MatchesPreset(settings, preset))
    {
      return preset.displayName;
    }
  }

  return "Custom";
}
} // namespace

std::string MriTractographySettings::GetInspectDisplayName() const
{
  return "Tractography Settings";
}

std::vector<std::shared_ptr<IInspectWidget>> MriTractographySettings::GetInspectFields()
{
  std::vector<std::shared_ptr<IInspectWidget>> fields;

  auto presetField = std::make_shared<InspectDropdownFieldWidget>(
    "tractographyPreset",
    "Preset",
    "Tractography",
    GetPresetOptions());
  presetField->SetValue(CurrentPresetName(*this));
  presetField->valueChangedCallback = [this](const QVariant &value)
  {
    const QString selectedPreset = value.toString();
    if (selectedPreset == "Custom")
    {
      return;
    }

    ApplyPreset(selectedPreset);
  };
  fields.push_back(presetField);

  fields.push_back(std::make_shared<InspectNumberFieldWidget>(
    "tractography.faSeedThreshold",
    "FA Seed Threshold",
    "Tractography",
    [this]() { return faSeedThresholdValue; },
    [this](double newValue) { faSeedThresholdValue = static_cast<float>(newValue); },
    0.0,
    1.0,
    0.01));

  fields.push_back(std::make_shared<InspectNumberFieldWidget>(
    "tractography.faStopThreshold",
    "FA Stop Threshold",
    "Tractography",
    [this]() { return faStopThresholdValue; },
    [this](double newValue) { faStopThresholdValue = static_cast<float>(newValue); },
    0.0,
    1.0,
    0.01));

  fields.push_back(std::make_shared<InspectNumberFieldWidget>(
    "tractography.l1StopThreshold",
    "L1 Stop Threshold",
    "Tractography",
    [this]() { return l1StopThresholdValue; },
    [this](double newValue) { l1StopThresholdValue = static_cast<float>(newValue); },
    0.0,
    1.0,
    0.000001));

  fields.push_back(std::make_shared<InspectNumberFieldWidget>(
    "tractography.stepSizeVoxels",
    "Step Size (voxels)",
    "Tractography",
    [this]() { return stepSizeVoxelsValue; },
    [this](double newValue) { stepSizeVoxelsValue = static_cast<float>(newValue); },
    0.001,
    10.0,
    0.01));

  fields.push_back(std::make_shared<InspectNumberFieldWidget>(
    "tractography.maxStepsPerStreamline",
    "Max Steps per Streamline",
    "Tractography",
    [this]() { return static_cast<double>(maxStepsPerStreamlineValue); },
    [this](double newValue) { maxStepsPerStreamlineValue = static_cast<int>(std::lround(newValue)); },
    1.0,
    10000.0,
    1.0));

  fields.push_back(std::make_shared<InspectNumberFieldWidget>(
    "tractography.seedStride",
    "Seed Stride",
    "Tractography",
    [this]() { return static_cast<double>(seedStrideValue); },
    [this](double newValue) { seedStrideValue = std::max(1, static_cast<int>(std::lround(newValue))); },
    1.0,
    64.0,
    1.0));

  fields.push_back(std::make_shared<InspectNumberFieldWidget>(
    "tractography.maxSeeds",
    "Max Seeds",
    "Tractography",
    [this]() { return static_cast<double>(maxSeedsValue); },
    [this](double newValue) { maxSeedsValue = static_cast<size_t>(std::max(1, static_cast<int>(std::lround(newValue)))); },
    1.0,
    100000.0,
    1.0));

  fields.push_back(std::make_shared<InspectNumberFieldWidget>(
    "tractography.minPointsPerStreamline",
    "Min Points per Streamline",
    "Tractography",
    [this]() { return static_cast<double>(minPointsPerStreamlineValue); },
    [this](double newValue) { minPointsPerStreamlineValue = static_cast<size_t>(std::max(1, static_cast<int>(std::lround(newValue)))); },
    1.0,
    10000.0,
    1.0));

  fields.push_back(std::make_shared<InspectNumberFieldWidget>(
    "tractography.tubeRadius",
    "Tube Radius",
    "Tractography",
    [this]() { return tubeRadiusValue; },
    [this](double newValue) { tubeRadiusValue = static_cast<float>(newValue); },
    0.00001,
    0.1,
    0.0001));

  fields.push_back(std::make_shared<InspectNumberFieldWidget>(
    "tractography.tubeRadialSegments",
    "Tube Radial Segments",
    "Tractography",
    [this]() { return static_cast<double>(tubeRadialSegmentsValue); },
    [this](double newValue) { tubeRadialSegmentsValue = static_cast<unsigned int>(std::max(3, static_cast<int>(std::lround(newValue)))); },
    3.0,
    32.0,
    1.0));

  return fields;
}

void MriTractographySettings::ApplyPreset(const QString &presetName)
{
  for (const PresetValues &preset : GetPresetValues())
  {
    if (presetName != preset.displayName)
    {
      continue;
    }

  faSeedThresholdValue = preset.faSeedThreshold;
  faStopThresholdValue = preset.faStopThreshold;
  l1StopThresholdValue = preset.l1StopThreshold;
  stepSizeVoxelsValue = preset.stepSizeVoxels;
  maxStepsPerStreamlineValue = preset.maxStepsPerStreamline;
  seedStrideValue = preset.seedStride;
  maxSeedsValue = preset.maxSeeds;
  minPointsPerStreamlineValue = preset.minPointsPerStreamline;
  tubeRadiusValue = preset.tubeRadius;
  tubeRadialSegmentsValue = preset.tubeRadialSegments;
    return;
  }
}
