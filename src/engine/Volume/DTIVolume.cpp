#include "DTIVolume.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "Texture3D.h"

namespace
{
  constexpr float kSpacingEpsilon = 1e-4f;

  bool NearlyEqual(float a, float b)
  {
    return std::fabs(a - b) <= kSpacingEpsilon;
  }

  bool HasFiniteVoxelData(const VolumeData<float>& volumeData)
  {
    const std::vector<float>& voxels = volumeData.GetVoxels();
    return std::all_of(voxels.begin(), voxels.end(),
      [](float v)
      {
        return std::isfinite(v) != 0;
      });
  }

  bool HasCompatibleMetadata(const VolumeData<float>& candidate,
                             const VolumeMetadata& reference)
  {
    const VolumeMetadata& metadata = candidate.GetMetadata();
    return metadata.dimensions == reference.dimensions &&
      NearlyEqual(metadata.spacing.x, reference.spacing.x) &&
      NearlyEqual(metadata.spacing.y, reference.spacing.y) &&
      NearlyEqual(metadata.spacing.z, reference.spacing.z);
  }

  void ValidateChannel(const std::optional<VolumeData<float>>& candidate,
                       const char* name,
                       const VolumeMetadata& reference)
  {
    if (!candidate.has_value())
    {
      return;
    }

    if (!HasCompatibleMetadata(*candidate, reference))
    {
      throw std::invalid_argument(std::string("DTI channel metadata mismatch: ") + name);
    }

    if (!HasFiniteVoxelData(*candidate))
    {
      throw std::invalid_argument(std::string("DTI channel contains non-finite values: ") + name);
    }
  }
}

/**
 * @brief Construct a new DTIVolume object from the provided DTI channels and shader. 
 *    The constructor validates the input channels for metadata consistency and finite voxel values, 
 *    and uploads available metrics to the GPU as textures.
 * 
 * @param channels 
 * @param shader 
 */
DTIVolume::DTIVolume(DTIVolumeChannels channels,
                     std::shared_ptr<Shader> shader)
  : Volume(channels.fa.GetMetadata(), std::move(shader)),
    channels(std::move(channels))
{
  const VolumeMetadata& reference = this->channels.fa.GetMetadata();
  if (reference.dimensions.x <= 0 || reference.dimensions.y <= 0 || reference.dimensions.z <= 0)
  {
    throw std::invalid_argument("DTIVolume requires positive dimensions.");
  }

  if (!HasFiniteVoxelData(this->channels.fa))
  {
    throw std::invalid_argument("DTI channel contains non-finite values: FA");
  }

  ValidateChannel(this->channels.md, "MD", reference);
  ValidateChannel(this->channels.ad, "AD", reference);
  ValidateChannel(this->channels.rd, "RD", reference);

  AddMetricTexture(Metric::FA, this->channels.fa);
  if (this->channels.md.has_value())
  {
    AddMetricTexture(Metric::MD, *this->channels.md);
  }
  if (this->channels.ad.has_value())
  {
    AddMetricTexture(Metric::AD, *this->channels.ad);
  }
  if (this->channels.rd.has_value())
  {
    AddMetricTexture(Metric::RD, *this->channels.rd);
  }

  SyncActiveMetricToAvailable();
}

/**
 * @brief Uniformprovider implementation to bind DTI-specific uniforms to the shader before drawing.
 * 
 * @param shader 
 */
void DTIVolume::Apply(Shader& shader) const
{
  Volume::Apply(shader);

  const int metricCount = GetAvailableMetricCount();
  const int textureIndex = GetTextureIndexForMetric(static_cast<Metric>(activeMetric));

  if (shader.HasUniform("dti.metricCount"))
  {
    shader.SetInt("dti.metricCount", metricCount);
  }
  if (shader.HasUniform("dti.activeMetric"))
  {
    shader.SetInt("dti.activeMetric", activeMetric);
  }
  if (shader.HasUniform("dti.activeMetricTexture"))
  {
    shader.SetInt("dti.activeMetricTexture", textureIndex);
  }
  if (shader.HasUniform("dti.threshold"))
  {
    shader.SetFloat("dti.threshold", metricThreshold);
  }
  if (shader.HasUniform("dti.opacityScale"))
  {
    shader.SetFloat("dti.opacityScale", opacityScale);
  }

  // Compatibility bridge for existing shaders that use shader.* uniforms.
  if (shader.HasUniform("shader.activeMetric"))
  {
    shader.SetInt("shader.activeMetric", activeMetric);
  }
  if (shader.HasUniform("shader.metricTexture"))
  {
    shader.SetInt("shader.metricTexture", textureIndex);
  }
  if (shader.HasUniform("shader.threshold"))
  {
    shader.SetFloat("shader.threshold", metricThreshold);
  }
  if (shader.HasUniform("shader.opacityScale"))
  {
    shader.SetFloat("shader.opacityScale", opacityScale);
  }
}

void DTIVolume::CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix)
{
  Volume::CollectInspectableFields(out, groupPrefix);

  const std::string group = groupPrefix.empty() ? "DTI" : (groupPrefix + "/DTI");

  UiField activeMetricField;
  activeMetricField.group = group;
  activeMetricField.label = "Active Metric";
  activeMetricField.kind = UiFieldKind::Int;
  activeMetricField.minInt = 0;
  activeMetricField.maxInt = kMetricCount - 1;
  activeMetricField.getter = [this]() -> UiFieldValue
  {
    return activeMetric;
  };
  activeMetricField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<int>(value))
    {
      return;
    }

    SetActiveMetric(std::get<int>(value));
  };
  out.push_back(std::move(activeMetricField));

  UiField thresholdField;
  thresholdField.group = group;
  thresholdField.label = "Metric Threshold";
  thresholdField.kind = UiFieldKind::Float;
  thresholdField.minFloat = 0.0f;
  thresholdField.maxFloat = 1.0f;
  thresholdField.speed = 0.005f;
  thresholdField.getter = [this]() -> UiFieldValue
  {
    return metricThreshold;
  };
  thresholdField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<float>(value))
    {
      return;
    }

    metricThreshold = std::clamp(std::get<float>(value), 0.0f, 1.0f);
  };
  out.push_back(std::move(thresholdField));

  UiField opacityField;
  opacityField.group = group;
  opacityField.label = "Opacity Scale";
  opacityField.kind = UiFieldKind::Float;
  opacityField.minFloat = 0.05f;
  opacityField.maxFloat = 4.0f;
  opacityField.speed = 0.01f;
  opacityField.getter = [this]() -> UiFieldValue
  {
    return opacityScale;
  };
  opacityField.setter = [this](const UiFieldValue& value)
  {
    if (!std::holds_alternative<float>(value))
    {
      return;
    }

    opacityScale = std::clamp(std::get<float>(value), 0.05f, 4.0f);
  };
  out.push_back(std::move(opacityField));
}

int DTIVolume::GetAvailableMetricCount() const
{
  int count = 0;
  for (int textureIndex : metricTextureIndices)
  {
    if (textureIndex >= 0)
    {
      ++count;
    }
  }

  return count;
}

int DTIVolume::GetActiveMetric() const
{
  return activeMetric;
}

void DTIVolume::SetActiveMetric(int metricIndex)
{
  activeMetric = std::clamp(metricIndex, 0, kMetricCount - 1);
  SyncActiveMetricToAvailable();
}

bool DTIVolume::HasMetric(Metric metric) const
{
  return GetTextureIndexForMetric(metric) >= 0;
}

int DTIVolume::GetTextureIndexForMetric(Metric metric) const
{
  return metricTextureIndices[ToMetricIndex(metric)];
}

const VolumeTextureSet& DTIVolume::GetTextureSet() const
{
  return textureSet;
}

int DTIVolume::ToMetricIndex(Metric metric)
{
  const int index = static_cast<int>(metric);
  if (index < 0 || index >= kMetricCount)
  {
    throw std::out_of_range("DTIVolume metric index out of range.");
  }

  return index;
}

const char* DTIVolume::MetricNameByIndex(int metricIndex)
{
  switch (metricIndex)
  {
    case 0:
      return "FA";
    case 1:
      return "MD";
    case 2:
      return "AD";
    case 3:
      return "RD";
    default:
      return "Unknown";
  }
}

void DTIVolume::AddMetricTexture(Metric metric, const VolumeData<float>& volumeData)
{
  const VolumeMetadata& metadata = volumeData.GetMetadata();
  const int textureIndex = static_cast<int>(textureSet.Size());

  textureSet.AddTexture(std::make_shared<Texture3D>(
    metadata.dimensions.x,
    metadata.dimensions.y,
    metadata.dimensions.z,
    GL_R32F,
    GL_RED,
    GL_FLOAT,
    volumeData.GetVoxels().data(),
    true));

  metricTextureIndices[ToMetricIndex(metric)] = textureIndex;
}

void DTIVolume::SyncActiveMetricToAvailable()
{
  if (metricTextureIndices[activeMetric] >= 0)
  {
    return;
  }

  for (int metricIndex = 0; metricIndex < kMetricCount; ++metricIndex)
  {
    if (metricTextureIndices[metricIndex] >= 0)
    {
      activeMetric = metricIndex;
      return;
    }
  }

  throw std::runtime_error("DTIVolume has no available metrics.");
}
