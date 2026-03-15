#pragma once

#include <glad/glad.h>

class Texture
{
public:
  virtual ~Texture() = default;

  virtual void Bind(unsigned int unit) const = 0;
  virtual bool IsValid() const = 0;
  virtual unsigned int GetId() const = 0;
};