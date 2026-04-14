#pragma once

#include <memory>

#include "Preprocessing/MriPreprocessingPipeline.h"

std::unique_ptr<IMriPreprocessingStage> CreateDwiInputValidationStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiGradientNormalizationStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiSkullExtractionStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiTensorSynthesisStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiPrincipalEigenvectorStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiScalarSynthesisStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiNormalizationStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiBrainSurfaceMeshStage();
std::unique_ptr<IMriPreprocessingStage> CreateDwiFiberTractographyStage();
