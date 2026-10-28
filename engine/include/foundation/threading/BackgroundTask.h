#pragma once

#include <atomic>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include "foundation/threading/Cancellation.h"

/**
 * @brief Runs one unit of work on its own std::thread, starting immediately on construction.
 *
 * The work receives a CancellationToken it is expected to poll. Owner-side calls (Cancel,
 * IsFinished, TakeResult, destruction) must come from a single owning thread. The work
 * function object itself (and anything it captured) is destroyed with the task, i.e. on the
 * owning thread, never on the worker - so captures may safely share ownership of objects that
 * must only be released on the owner's thread.
 */
template <typename T>
class BackgroundTask
{
public:
  using Work = std::function<T(const CancellationToken&)>;

  explicit BackgroundTask(Work work)
    : work(std::move(work)),
      worker(&BackgroundTask::Run, this)
  {
  }

  // Cancels and joins: destroying a task blocks until its work notices the cancellation.
  ~BackgroundTask()
  {
    Cancel();
    if (worker.joinable())
    {
      worker.join();
    }
  }

  BackgroundTask(const BackgroundTask&) = delete;
  BackgroundTask& operator=(const BackgroundTask&) = delete;

  void Cancel() { cancellation.Cancel(); }

  bool IsFinished() const { return finished.load(std::memory_order_acquire); }

  // Joins the worker, then returns the result or rethrows the exception the work exited with.
  // May only be called once.
  T TakeResult()
  {
    if (worker.joinable())
    {
      worker.join();
    }

    if (std::exception_ptr pendingError = std::exchange(error, nullptr))
    {
      std::rethrow_exception(pendingError);
    }

    if (!result.has_value())
    {
      throw std::logic_error("BackgroundTask result was already taken.");
    }

    T value = std::move(*result);
    result.reset();
    return value;
  }

private:
  void Run()
  {
    try
    {
      result.emplace(work(cancellation.Token()));
    }
    catch (...)
    {
      error = std::current_exception();
    }

    finished.store(true, std::memory_order_release);
  }

  Work work;
  CancellationSource cancellation;
  std::optional<T> result;
  std::exception_ptr error;
  std::atomic<bool> finished{false};

  // Declared last so every member above is constructed before the worker starts running.
  std::thread worker;
};
