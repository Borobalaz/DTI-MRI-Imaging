#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

class MriToDtiPreprocessor
{
public:
  MriToDtiPreprocessor();

  // Stateless and const, so one instance may be shared by concurrent runs on worker threads.
  MriPreprocessingResult Process(const MriPreprocessingRequest& request,
                                 const MriPreprocessingExecutionOptions& options = {}) const;

private:
  MriPreprocessingPipeline pipeline;
};