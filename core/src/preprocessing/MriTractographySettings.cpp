#include "Preprocessing/MriTractographySettings.h"

#include <algorithm>
#include <cmath>

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

std::vector<std::string> GetPresetOptions()
{
  std::vector<std::string> options{"Custom"};
  for (const PresetValues &preset : GetPresetValues())
  {
    options.emplace_back(preset.displayName);
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

std::string CurrentPresetName(const MriTractographySettings &settings)
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

InspectFieldPtr MakeNumberField(std::string id,
                                std::string displayName,
                                const std::function<double()>& getter,
                                std::function<void(double)> setter,
                                double minimum,
                                double maximum,
                                double step)
{
  auto field = MakeInspectField(
      std::move(id), std::move(displayName), "Tractography", InspectFieldType::Number,
      getter(),
      [getter]() -> InspectValue { return getter(); },
      [setter = std::move(setter)](const InspectValue& value)
      {
        if (const auto* number = std::get_if<double>(&value)) setter(*number);
      });
  field->minimum = minimum;
  field->maximum = maximum;
  field->step = step;
  return field;
}
} // namespace

std::string MriTractographySettings::GetInspectDisplayName() const
{
  return "Tractography Settings";
}

std::string MriTractographySettings::GetMatchedPresetName() const
{
  return CurrentPresetName(*this);
}

std::vector<InspectFieldPtr> MriTractographySettings::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields;

  auto presetField = MakeInspectField(
      "tractographyPreset", "Preset", "Tractography", InspectFieldType::Dropdown,
      CurrentPresetName(*this),
      [this]() -> InspectValue { return CurrentPresetName(*this); },
      [this](const InspectValue& value)
      {
        const auto* selectedPreset = std::get_if<std::string>(&value);
        if (selectedPreset && *selectedPreset != "Custom") ApplyPreset(*selectedPreset);
      });
  presetField->enumOptions = GetPresetOptions();
  fields.push_back(presetField);

  fields.push_back(MakeNumberField(
    "tractography.faSeedThreshold", "FA Seed Threshold",
    [this]() { return static_cast<double>(faSeedThresholdValue); },
    [this](double value) { faSeedThresholdValue = static_cast<float>(value); },
    0.0, 1.0, 0.01));

  fields.push_back(MakeNumberField(
    "tractography.faStopThreshold", "FA Stop Threshold",
    [this]() { return static_cast<double>(faStopThresholdValue); },
    [this](double value) { faStopThresholdValue = static_cast<float>(value); },
    0.0, 1.0, 0.01));

  fields.push_back(MakeNumberField(
    "tractography.l1StopThreshold", "L1 Stop Threshold",
    [this]() { return static_cast<double>(l1StopThresholdValue); },
    [this](double value) { l1StopThresholdValue = static_cast<float>(value); },
    0.0, 1.0, 0.000001));

  fields.push_back(MakeNumberField(
    "tractography.stepSizeVoxels", "Step Size (voxels)",
    [this]() { return static_cast<double>(stepSizeVoxelsValue); },
    [this](double value) { stepSizeVoxelsValue = static_cast<float>(value); },
    0.001, 10.0, 0.01));

  fields.push_back(MakeNumberField(
    "tractography.maxStepsPerStreamline", "Max Steps per Streamline",
    [this]() { return static_cast<double>(maxStepsPerStreamlineValue); },
    [this](double value) { maxStepsPerStreamlineValue = static_cast<int>(std::lround(value)); },
    1.0, 10000.0, 1.0));

  fields.push_back(MakeNumberField(
    "tractography.seedStride", "Seed Stride",
    [this]() { return static_cast<double>(seedStrideValue); },
    [this](double value) { seedStrideValue = std::max(1, static_cast<int>(std::lround(value))); },
    1.0, 64.0, 1.0));

  fields.push_back(MakeNumberField(
    "tractography.maxSeeds", "Max Seeds",
    [this]() { return static_cast<double>(maxSeedsValue); },
    [this](double value) { maxSeedsValue = static_cast<size_t>(std::max(1, static_cast<int>(std::lround(value)))); },
    1.0, 100000.0, 1.0));

  fields.push_back(MakeNumberField(
    "tractography.minPointsPerStreamline", "Min Points per Streamline",
    [this]() { return static_cast<double>(minPointsPerStreamlineValue); },
    [this](double value) { minPointsPerStreamlineValue = static_cast<size_t>(std::max(1, static_cast<int>(std::lround(value)))); },
    1.0, 10000.0, 1.0));

  fields.push_back(MakeNumberField(
    "tractography.tubeRadius", "Tube Radius",
    [this]() { return static_cast<double>(tubeRadiusValue); },
    [this](double value) { tubeRadiusValue = static_cast<float>(value); },
    0.00001, 0.1, 0.0001));

  fields.push_back(MakeNumberField(
    "tractography.tubeRadialSegments", "Tube Radial Segments",
    [this]() { return static_cast<double>(tubeRadialSegmentsValue); },
    [this](double value) { tubeRadialSegmentsValue = static_cast<unsigned int>(std::max(3, static_cast<int>(std::lround(value)))); },
    3.0, 32.0, 1.0));

  return fields;
}

void MriTractographySettings::ApplyPreset(const std::string &presetName)
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
