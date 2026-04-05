#pragma once

#include <memory>

#include "Preprocessing/MriPreprocessingPipeline.h"

std::unique_ptr<IMriPreprocessingStage> CreateDwiGradientNormalizationStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiTensorSynthesisStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiNormalizationStage();