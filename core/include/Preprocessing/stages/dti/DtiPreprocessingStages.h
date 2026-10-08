#pragma once

#include <memory>

#include "Preprocessing/MriPreprocessingPipeline.h"

std::unique_ptr<IMriPreprocessingStage> CreateDtiTensorSynthesisStage();
std::unique_ptr<IMriPreprocessingStage> CreateDtiPrincipalEigenvectorStage();
std::unique_ptr<IMriPreprocessingStage> CreateDtiScalarSynthesisStage();
std::unique_ptr<IMriPreprocessingStage> CreateDtiFiberTractographyStage();
