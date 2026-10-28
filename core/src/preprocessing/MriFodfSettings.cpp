#include "Preprocessing/MriFodfSettings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace
{
constexpr const char *kCategory = "fODF (CSD)";

const std::array<const char *, 4> kLmaxOptions = {"2", "4", "6", "8"};

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

void MriFodfSettings::SetLmax(int value)
{
  value = std::clamp(value, 2, 8);
  value -= value % 2;
  lmaxValue = value;
}

void MriFodfSettings::SetResponseVoxelCount(int value)
{
  responseVoxelCountValue = std::clamp(value, 10, 10000);
}

void MriFodfSettings::SetResponseFaFloor(float value)
{
  responseFaFloorValue = std::clamp(value, 0.0f, 1.0f);
}

void MriFodfSettings::SetNegativityThresholdTau(float value)
{
  negativityThresholdTauValue = std::clamp(value, 0.0f, 1.0f);
}

void MriFodfSettings::SetLambda(float value)
{
  lambdaValue = std::clamp(value, 0.0f, 100.0f);
}

void MriFodfSettings::SetMaxIterations(int value)
{
  maxIterationsValue = std::clamp(value, 1, 500);
}

void MriFodfSettings::SetShellTolerance(float value)
{
  shellToleranceValue = std::clamp(value, 1.0f, 1000.0f);
}

std::string MriFodfSettings::GetInspectDisplayName() const
{
  return "fODF Settings";
}

std::vector<InspectFieldPtr> MriFodfSettings::GetInspectFields()
{
  std::vector<InspectFieldPtr> fields;

  auto lmaxField = MakeInspectField(
      "fodf.lmax", "Max SH Order (lmax)", kCategory, InspectFieldType::Dropdown,
      std::to_string(lmaxValue),
      [this]() -> InspectValue { return std::to_string(lmaxValue); },
      [this](const InspectValue& value)
      {
        const auto* selected = std::get_if<std::string>(&value);
        if (!selected) return;
        for (size_t i = 0; i < kLmaxOptions.size(); ++i)
        {
          if (*selected == kLmaxOptions[i])
          {
            SetLmax(static_cast<int>(2 * (i + 1)));
            return;
          }
        }
      });
  lmaxField->enumOptions = {"2", "4", "6", "8"};
  fields.push_back(lmaxField);

  fields.push_back(MakeNumberField(
    "fodf.responseVoxelCount", "Response Voxel Count",
    [this]() { return static_cast<double>(responseVoxelCountValue); },
    [this](double value) { SetResponseVoxelCount(static_cast<int>(std::lround(value))); },
    10.0, 10000.0, 1.0));

  fields.push_back(MakeNumberField(
    "fodf.responseFaFloor", "Response FA Floor",
    [this]() { return static_cast<double>(responseFaFloorValue); },
    [this](double value) { SetResponseFaFloor(static_cast<float>(value)); },
    0.0, 1.0, 0.01));

  fields.push_back(MakeNumberField(
    "fodf.negativityThresholdTau", "Negativity Threshold (tau)",
    [this]() { return static_cast<double>(negativityThresholdTauValue); },
    [this](double value) { SetNegativityThresholdTau(static_cast<float>(value)); },
    0.0, 1.0, 0.01));

  fields.push_back(MakeNumberField(
    "fodf.lambda", "Lambda",
    [this]() { return static_cast<double>(lambdaValue); },
    [this](double value) { SetLambda(static_cast<float>(value)); },
    0.0, 100.0, 0.1));

  fields.push_back(MakeNumberField(
    "fodf.maxIterations", "Max Iterations",
    [this]() { return static_cast<double>(maxIterationsValue); },
    [this](double value) { SetMaxIterations(static_cast<int>(std::lround(value))); },
    1.0, 500.0, 1.0));

  fields.push_back(MakeNumberField(
    "fodf.shellTolerance", "Shell Tolerance (s/mm^2)",
    [this]() { return static_cast<double>(shellToleranceValue); },
    [this](double value) { SetShellTolerance(static_cast<float>(value)); },
    1.0, 1000.0, 10.0));

  return fields;
}
