#pragma once

#include <memory>

#include "Preprocessing/MriPreprocessingPipeline.h"

std::unique_ptr<IMriPreprocessingStage> CreateDwiInputValidationStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiGradientNormalizationStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiNormalizationStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiBrainSurfaceMeshStage();
