#pragma once

#include <memory>

#include "Preprocessing/MriPreprocessingPipeline.h"

std::unique_ptr<IMriPreprocessingStage> CreateDatasetDiscoveryStage();
std::unique_ptr<IMriPreprocessingStage> CreateScalarVolumeLoadStage();
std::unique_ptr<IMriPreprocessingStage> CreateIntensityNormalizationStage();
std::unique_ptr<IMriPreprocessingStage> CreateDerivedDtiChannelSynthesisStage();
