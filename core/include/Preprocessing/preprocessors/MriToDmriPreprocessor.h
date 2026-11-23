#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

// Unified diffusion-MRI preprocessor: the full DTI pipeline (channels, tensor-ODF SH, surface,
// DTI streamlines and seeds) followed by the fODF stages (shell selection, response, CSD, peak
// extraction, fODF tractography). Every stage is mandatory: any stage failure fails the run, and
// cancellation cancels it.
class MriToDmriPreprocessor
{
public:
  MriToDmriPreprocessor();

  // Stateless and const, so one instance may be shared by concurrent runs on worker threads.
  MriPreprocessingResult Process(const MriPreprocessingRequest& request,
                                 const MriPreprocessingExecutionOptions& options = {}) const;

private:
  MriPreprocessingPipeline pipeline;
};
