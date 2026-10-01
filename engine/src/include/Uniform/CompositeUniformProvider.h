#pragma once

#include <vector>

#include "Uniform/UniformProvider.h"

class CompositeUniformProvider : public UniformProvider
{
public:
  void AddProvider(const UniformProvider& provider);
  void ClearProviders();

  void Apply(Shader& shader) const override;

private:
  std::vector<const UniformProvider*> providers;
};