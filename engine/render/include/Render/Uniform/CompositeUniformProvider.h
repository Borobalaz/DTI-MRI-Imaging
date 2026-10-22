#pragma once

#include <vector>

#include "Render/Uniform/UniformProvider.h"

class CompositeUniformProvider : public UniformProvider
{
public:
  void AddProvider(const UniformProvider& provider);
  void ClearProviders();

  void Apply(IUniformSink& shader) const override;

private:
  std::vector<const UniformProvider*> providers;
};