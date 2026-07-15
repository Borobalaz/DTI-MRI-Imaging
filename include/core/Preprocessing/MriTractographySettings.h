#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <QString>

#include "ui/widgets/inspect_fields/InspectProvider.h"

class MriTractographySettings final : public InspectProvider
{
public:
  MriTractographySettings() = default;

  float GetFaSeedThreshold() const { return faSeedThresholdValue; }
  float GetFaStopThreshold() const { return faStopThresholdValue; }
  float GetL1StopThreshold() const { return l1StopThresholdValue; }
  float GetStepSizeVoxels() const { return stepSizeVoxelsValue; }
  int GetMaxStepsPerStreamline() const { return maxStepsPerStreamlineValue; }
  int GetSeedStride() const { return seedStrideValue; }
  size_t GetMaxSeeds() const { return maxSeedsValue; }
  size_t GetMinPointsPerStreamline() const { return minPointsPerStreamlineValue; }
  float GetTubeRadius() const { return tubeRadiusValue; }
  unsigned int GetTubeRadialSegments() const { return tubeRadialSegmentsValue; }

  void SetFaSeedThreshold(float value) { faSeedThresholdValue = value; }
  void SetFaStopThreshold(float value) { faStopThresholdValue = value; }
  void SetL1StopThreshold(float value) { l1StopThresholdValue = value; }
  void SetStepSizeVoxels(float value) { stepSizeVoxelsValue = value; }
  void SetMaxStepsPerStreamline(int value) { maxStepsPerStreamlineValue = value; }
  void SetSeedStride(int value) { seedStrideValue = std::max(1, value); }
  void SetMaxSeeds(size_t value) { maxSeedsValue = std::max<size_t>(1, value); }
  void SetMinPointsPerStreamline(size_t value) { minPointsPerStreamlineValue = std::max<size_t>(1, value); }
  void SetTubeRadius(float value) { tubeRadiusValue = value; }
  void SetTubeRadialSegments(unsigned int value) { tubeRadialSegmentsValue = std::max<unsigned int>(3U, value); }

  std::string GetInspectDisplayName() const override;
  std::vector<std::shared_ptr<IInspectWidget>> GetInspectFields() override;

private:
  void ApplyPreset(const QString &presetName);

  float faSeedThresholdValue = 0.4f;
  float faStopThresholdValue = 0.3f;
  float l1StopThresholdValue = 1e-6f;
  float stepSizeVoxelsValue = 0.3f;
  int maxStepsPerStreamlineValue = 250;
  int seedStrideValue = 1;
  size_t maxSeedsValue = 3000;
  size_t minPointsPerStreamlineValue = 15;
  float tubeRadiusValue = 0.001f;
  unsigned int tubeRadialSegmentsValue = 3;
};