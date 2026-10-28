#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Preprocessing/MriPreprocessingPipeline.h"

std::unique_ptr<IMriPreprocessingStage> CreateFodfResponseEstimationStage();
std::unique_ptr<IMriPreprocessingStage> CreateFodfCsdStage();

// Synthetic CSD check on the given gradient scheme (no test framework exists, so the CSD stage
// runs it once per run): fits a response from synthetic single fibres, deconvolves noise-free
// 90 and 60 degree crossings and checks the recovered peaks. Logs one line (tag "FodfCsd") and
// returns whether it passed; never throws.
bool SelfTestCsd(const std::vector<glm::vec3>& shellDirs, float bValue, int lmax);
