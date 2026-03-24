#pragma once

#include <array>
#include <optional>
#include <string>

#include "Volume.h"
#include "VolumeTextureSet.h"

struct DTIVolumeChannels
{
  VolumeData<float> fa;
  std::optional<VolumeData<float>> md;
  std::optional<VolumeData<float>> ad;
  std::optional<VolumeData<float>> rd;
};

class DTIVolume final : public Volume
{
public:
  enum class Metric : int
  {
    FA = 0,
    MD = 1,
    AD = 2,
    RD = 3
  };

  explicit DTIVolume(DTIVolumeChannels channels,
                     std::shared_ptr<Shader> shader);

  void Apply(Shader& shader) const override;
  void CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix) override;

  int GetAvailableMetricCount() const;
  int GetActiveMetric() const;
  void SetActiveMetric(int metricIndex);

  bool HasMetric(Metric metric) const;
  int GetTextureIndexForMetric(Metric metric) const;

private:
  const VolumeTextureSet& GetTextureSet() const override;

  static constexpr int kMetricCount = 4;

  static int ToMetricIndex(Metric metric);
  static const char* MetricNameByIndex(int metricIndex);

  void AddMetricTexture(Metric metric, const VolumeData<float>& volumeData);
  void SyncActiveMetricToAvailable();

  DTIVolumeChannels channels;
  VolumeTextureSet textureSet;

  std::array<int, kMetricCount> metricTextureIndices{{-1, -1, -1, -1}};
  int activeMetric = 0;

  // Shader-agnostic bridge controls exposed through the inspectable UI.
  float metricThreshold = 0.15f;
  float opacityScale = 1.0f;
};