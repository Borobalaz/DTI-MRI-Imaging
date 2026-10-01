#pragma once

#include <string>

class Shader;

class UniformProvider
{
public:
  virtual ~UniformProvider() = default;

  virtual void Apply(Shader& shader) const = 0;

protected:
  static std::string ComposeUniformName(const std::string& className,
                                        const std::string& fieldName);
};