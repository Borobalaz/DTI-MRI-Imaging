#pragma once

#include <mutex>
#include <string>

struct ProgressSnapshot
{
  std::string label;
  int step = 0;       // 1-based step currently running; 0 when no step structure applies
  int stepCount = 0;  // 0 when the work has no step structure
  float fraction = 0.0f; // overall completion in [0,1]
};

/**
 * @brief Latest-value progress mailbox between a worker (Publish) and an observer (Read),
 *        each of which may run on a different thread.
 */
class SharedProgress
{
public:
  void Publish(ProgressSnapshot progress)
  {
    std::lock_guard<std::mutex> lock(mutex);
    snapshot = std::move(progress);
  }

  ProgressSnapshot Read() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    return snapshot;
  }

private:
  mutable std::mutex mutex;
  ProgressSnapshot snapshot;
};
