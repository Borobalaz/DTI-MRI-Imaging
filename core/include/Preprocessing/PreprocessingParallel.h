#pragma once

#include <functional>

struct MriPreprocessingContext;

// Runs body(z) for every z in [0, depth) on max(1, hardware_concurrency()-1) worker threads (capped at depth).
// The calling thread only waits: it polls cancellation and calls ReportStageFraction(completedSlices / depth) on itself,
// never on a worker. body runs on worker threads and must not call ThrowIfCancelled/ReportStageFraction or touch
// context.report; it may only write output slots owned by its slice. Throws MriPreprocessingCancelled on cancellation
// (after all workers joined) or rethrows the first exception a body threw.
void ParallelForSlices(MriPreprocessingContext& context, int depth, const std::function<void(int z)>& body);
