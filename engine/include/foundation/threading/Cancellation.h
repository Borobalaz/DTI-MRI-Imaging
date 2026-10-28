#pragma once

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>

/**
 * @brief Thrown by cooperative cancellation checks; callers treat it as "stopped on request",
 *        not as a failure.
 */
class OperationCancelled : public std::runtime_error
{
public:
  OperationCancelled() : std::runtime_error("Operation cancelled") {}
  explicit OperationCancelled(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @brief Read-only view of a CancellationSource's flag, safe to copy across threads.
 *        A default-constructed token never reports cancellation.
 */
class CancellationToken
{
public:
  CancellationToken() = default;

  bool IsCancellationRequested() const
  {
    return cancelled && cancelled->load(std::memory_order_acquire);
  }

  void ThrowIfCancellationRequested() const
  {
    if (IsCancellationRequested())
    {
      throw OperationCancelled();
    }
  }

private:
  friend class CancellationSource;

  explicit CancellationToken(std::shared_ptr<const std::atomic<bool>> cancelled)
    : cancelled(std::move(cancelled))
  {
  }

  std::shared_ptr<const std::atomic<bool>> cancelled;
};

/**
 * @brief Owner side of a cooperative cancellation flag; hands out CancellationTokens that
 *        observe it. Cancel() may be called from any thread and is idempotent.
 */
class CancellationSource
{
public:
  CancellationSource() : cancelled(std::make_shared<std::atomic<bool>>(false)) {}

  void Cancel() { cancelled->store(true, std::memory_order_release); }
  bool IsCancellationRequested() const { return cancelled->load(std::memory_order_acquire); }
  CancellationToken Token() const { return CancellationToken(cancelled); }

private:
  std::shared_ptr<std::atomic<bool>> cancelled;
};
