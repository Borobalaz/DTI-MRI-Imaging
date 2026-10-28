#pragma once

#include <string>

#include "render/uniform/IUniformSink.h"

class UniformProvider
{
public:
  virtual ~UniformProvider() = default;

  virtual void Apply(IUniformSink& shader) const = 0;

protected:
  static std::string ComposeUniformName(const std::string& className,
                                        const std::string& fieldName);
};