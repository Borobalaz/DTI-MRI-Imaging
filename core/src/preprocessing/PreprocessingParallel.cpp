#include "Preprocessing/PreprocessingParallel.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

#include "Preprocessing/MriPreprocessingPipeline.h"

namespace
{
  // Owns the worker threads; on every exit path (including a failed spawn) it raises the stop flag
  // and joins whatever was started, so no worker outlives the shared locals it references.
  class WorkerThreadGuard
  {
  public:
    explicit WorkerThreadGuard(std::atomic<bool>& stopFlag) : stop(stopFlag) {}
    ~WorkerThreadGuard()
    {
      stop.store(true, std::memory_order_release);
      JoinAll();
    }

    WorkerThreadGuard(const WorkerThreadGuard&) = delete;
    WorkerThreadGuard& operator=(const WorkerThreadGuard&) = delete;

    std::vector<std::thread>& Threads() { return threads; }

    void JoinAll()
    {
      for (std::thread& thread : threads)
      {
        if (thread.joinable())
        {
          thread.join();
        }
      }
    }

  private:
    std::atomic<bool>& stop;
    std::vector<std::thread> threads;
  };
}

/**
 * @brief Run body(z) for every slice z in [0, depth) on a small worker pool while the calling
 *        thread polls cancellation and reports stage progress.
 *
 * @param context pipeline context (cancellation token and progress callback)
 * @param depth number of slices
 * @param body per-slice work; runs on worker threads
 */
void ParallelForSlices(MriPreprocessingContext& context, int depth, const std::function<void(int z)>& body)
{
  if (depth <= 0)
  {
    return;
  }

  unsigned int hardwareThreads = std::thread::hardware_concurrency();
  if (hardwareThreads == 0)
  {
    hardwareThreads = 2;
  }
  const int workerCount = std::min(depth, std::max(1, static_cast<int>(hardwareThreads) - 1));

  std::atomic<int> nextZ{0};
  std::atomic<int> completed{0};
  std::atomic<int> finishedWorkers{0};
  std::atomic<bool> stop{false};
  std::mutex mutex;
  std::condition_variable cv;
  std::exception_ptr firstError;

  const CancellationToken& cancellation = context.cancellation;
  auto worker = [&]()
  {
    while (!stop.load(std::memory_order_acquire))
    {
      if (cancellation.IsCancellationRequested())
      {
        stop.store(true, std::memory_order_release);
        break;
      }

      const int z = nextZ.fetch_add(1);
      if (z >= depth)
      {
        break;
      }

      try
      {
        body(z);
      }
      catch (...)
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (!firstError)
        {
          firstError = std::current_exception();
        }
        stop.store(true, std::memory_order_release);
        break;
      }
      completed.fetch_add(1, std::memory_order_release);
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      ++finishedWorkers;
    }
    cv.notify_one();
  };

  WorkerThreadGuard guard(stop);
  guard.Threads().reserve(static_cast<size_t>(workerCount));
  for (int i = 0; i < workerCount; ++i)
  {
    guard.Threads().emplace_back(worker);
  }

  int lastReported = -1;
  while (true)
  {
    {
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait_for(lock, std::chrono::milliseconds(50), [&]() { return finishedWorkers.load() == workerCount; });
    }

    if (context.cancellation.IsCancellationRequested())
    {
      stop.store(true, std::memory_order_release);
      break;
    }

    const int done = completed.load(std::memory_order_acquire);
    if (done != lastReported)
    {
      context.ReportStageFraction(static_cast<float>(done) / static_cast<float>(depth));
      lastReported = done;
    }

    if (finishedWorkers.load() == workerCount)
    {
      break;
    }
  }

  guard.JoinAll();
  if (firstError)
  {
    // A real failure takes precedence over a cancellation.
    std::rethrow_exception(firstError);
  }
  context.ThrowIfCancelled();
}
