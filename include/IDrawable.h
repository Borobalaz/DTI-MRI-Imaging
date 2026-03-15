#pragma once

class UniformProvider;

class IDrawable
{
public:
  virtual ~IDrawable() = default;

  virtual void Draw(const UniformProvider& frameUniforms) const = 0;
};