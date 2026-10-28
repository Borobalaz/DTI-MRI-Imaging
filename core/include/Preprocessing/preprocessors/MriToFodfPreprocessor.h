#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

class MriToFodfPreprocessor
{
public:
  MriToFodfPreprocessor();

  // Stateless and const, so one instance may be shared by concurrent runs on worker threads.
  MriPreprocessingResult Process(const MriPreprocessingRequest& request,
                                 const MriPreprocessingExecutionOptions& options = {}) const;

private:
  MriPreprocessingPipeline pipeline;
};
