#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

class MriToDtiPreprocessor
{
public:
  MriToDtiPreprocessor();

  MriPreprocessingResult Process(const MriPreprocessingRequest& request) const;

private:
  MriPreprocessingPipeline pipeline;
};