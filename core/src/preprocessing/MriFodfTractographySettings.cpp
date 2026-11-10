#include "Preprocessing/MriFodfTractographySettings.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace
{
constexpr const char *kCategory = "fODF Tractography";

InspectFieldPtr MakeNumberField(std::string id,
                                std::string displayName,
                                const std::function<double()>& getter,
                                std::function<void(double)> setter,
                                double minimum,
                                double maximum,
                                double step)
{
  auto field = MakeInspectField(
      std::move(id), std::move(displayName), kCategory, InspectFieldType::Number,
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

void MriFodfTractographySettings::SetRelativeAmplitudeCutoff(float value)
{
  relativeAmplitudeCutoffValue = std::clamp(value, 0.0f, 1.0f);
}

void MriFodfTractographySettings::SetMaxAngleDegrees(float value)
{
  maxAngleDegreesValue = std::clamp(value, 1.0f, 90.0f);
}

std::string MriFodfTractographySettings::GetInspectDisplayName() const
{
  return "fODF Tractography Settings";
}

std::vector<InspectFieldPtr> MriFodfTractographySettings::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields;

  fields.push_back(MakeNumberField(
    "fodfTractography.relativeAmplitudeCutoff", "Amplitude Cutoff (relative)",
    [this]() { return static_cast<double>(relativeAmplitudeCutoffValue); },
    [this](double value) { SetRelativeAmplitudeCutoff(static_cast<float>(value)); },
    0.0, 1.0, 0.01));

  fields.push_back(MakeNumberField(
    "fodfTractography.maxAngleDegrees", "Max Angle per Step (deg)",
    [this]() { return static_cast<double>(maxAngleDegreesValue); },
    [this](double value) { SetMaxAngleDegrees(static_cast<float>(value)); },
    1.0, 90.0, 1.0));

  return fields;
}
