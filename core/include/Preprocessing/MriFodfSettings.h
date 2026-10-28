#pragma once

#include <string>
#include <vector>

#include "inspection/InspectField.h"

// CSD (Tournier 2007) parameters for MriToFodfPreprocessor; editable through the inspector
// (category "fODF (CSD)", ids "fodf.*"). Setters clamp; copyable so a run can snapshot it.
class MriFodfSettings final : public InspectProvider
{
public:
  MriFodfSettings() = default;

  int GetLmax() const { return lmaxValue; }
  int GetResponseVoxelCount() const { return responseVoxelCountValue; }
  float GetResponseFaFloor() const { return responseFaFloorValue; }
  float GetNegativityThresholdTau() const { return negativityThresholdTauValue; }
  float GetLambda() const { return lambdaValue; }
  int GetMaxIterations() const { return maxIterationsValue; }
  float GetShellTolerance() const { return shellToleranceValue; }

  void SetLmax(int value);                       // clamp [2,8], then round down to even
  void SetResponseVoxelCount(int value);         // clamp [10,10000]
  void SetResponseFaFloor(float value);          // clamp [0,1]
  void SetNegativityThresholdTau(float value);   // clamp [0,1]
  void SetLambda(float value);                   // clamp [0,100]
  void SetMaxIterations(int value);              // clamp [1,500]
  void SetShellTolerance(float value);           // clamp [1,1000]

  std::string GetInspectDisplayName() const override;   // returns "fODF Settings"
  std::vector<InspectFieldPtr> GetInspectFields() override;

private:
  int lmaxValue = 8;
  int responseVoxelCountValue = 300;
  float responseFaFloorValue = 0.7f;
  float negativityThresholdTauValue = 0.1f;
  float lambdaValue = 1.0f;
  int maxIterationsValue = 50;
  float shellToleranceValue = 100.0f;
};
