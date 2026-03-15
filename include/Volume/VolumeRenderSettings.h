#pragma once

enum class VolumeRenderMode
{
  ScalarDensity = 0,
  MatrixTrace = 1,
  MatrixDeterminant = 2,
  MatrixFrobeniusNorm = 3,
  MatrixComponent00 = 4,
  MatrixComponent11 = 5,
  MatrixComponent22 = 6
};

struct VolumeRenderSettings
{
  float stepSize = 0.01f;
  float opacityScale = 0.35f;
  float intensityScale = 1.0f;
  float threshold = 0.05f;
  int maxSteps = 256;
  VolumeRenderMode renderMode = VolumeRenderMode::ScalarDensity;
};
