#include "render/uniform/CompositeUniformProvider.h"

void CompositeUniformProvider::AddProvider(const UniformProvider& provider)
{
  providers.push_back(&provider);
}

void CompositeUniformProvider::ClearProviders()
{
  providers.clear();
}

void CompositeUniformProvider::Apply(IUniformSink& shader) const
{
  for (const UniformProvider* provider : providers)
  {
    if (provider != nullptr)
    {
      provider->Apply(shader);
    }
  }
}