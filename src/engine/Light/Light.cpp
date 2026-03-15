#include "Light.h"

void Light::SetUniformIndex(int index)
{
  uniformIndex = index;
}

int Light::GetUniformIndex() const
{
  return uniformIndex;
}
