#pragma once

#include "Preprocessing/MriPreprocessingPipeline.h"

// Unified diffusion-MRI preprocessor: the full DTI pipeline (channels, tensor-ODF SH, surface,
// DTI streamlines and seeds) plus an optional "fODF reconstruction" group (shell selection,
// response, CSD, peak extraction, fODF tractography). A failure inside the group degrades the run
// to DTI-only: the fODF outputs are cleared and MriPreprocessingResult::fodfUnavailableReason says
// why. Cancellation still cancels the whole run.
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
