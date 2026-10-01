#pragma once

#include <array>
#include <vector>

#include <glm/vec3.hpp>

#include "Preprocessing/MriPreprocessingPipeline.h"

class DWITensorSynthesisStage final : public IMriPreprocessingStage
{
public:
  const char* Name() const override;
  void Execute(MriPreprocessingContext& context) const override;

private:
  static std::array<float, 6> BuildDesignRow(float b, const glm::vec3& g);
  static std::array<float, 6> SolveSymmetricSystem6x6(
    std::array<std::array<float, 6>, 6> normalMatrix,
    std::array<float, 6> rhs);

  std::array<float, 6> EstimateTensorFromSignal(
    const std::vector<float>& dwiSignal,
    const std::vector<float>& bValues,
    const std::vector<glm::vec3>& gradients) const;
};
