#pragma once

#include <string>
#include <vector>

#include "inspection/InspectField.h"

// fODF peak-following tractography parameters for FodfFiberTractographyStage; editable through the
// inspector (category "fODF Tractography", ids "fodfTractography.*"). Step size, step count, minimum
// points and tube settings are shared with DTI tracking and live in MriTractographySettings.
// Setters clamp; copyable so a run can snapshot it.
class MriFodfTractographySettings final : public InspectProvider
{
public:
  MriFodfTractographySettings() = default;

  float GetRelativeAmplitudeCutoff() const { return relativeAmplitudeCutoffValue; }
  float GetMaxAngleDegrees() const { return maxAngleDegreesValue; }

  void SetRelativeAmplitudeCutoff(float value);   // clamp [0,1]
  void SetMaxAngleDegrees(float value);           // clamp [1,90]

  std::string GetInspectDisplayName() const override;   // returns "fODF Tractography Settings"
  std::vector<InspectFieldPtr> GetInspectFields() override;

private:
  // Fraction of the 95th percentile of peak-1 amplitude below which a peak is ignored.
  float relativeAmplitudeCutoffValue = 0.1f;
  float maxAngleDegreesValue = 45.0f;
};
