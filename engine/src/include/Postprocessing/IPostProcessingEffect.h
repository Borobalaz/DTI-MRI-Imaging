#pragma once


class IPostProcessingEffect
{
public:
  virtual void Apply(unsigned int inputFramebuffer, unsigned int outputFramebuffer) = 0;
};