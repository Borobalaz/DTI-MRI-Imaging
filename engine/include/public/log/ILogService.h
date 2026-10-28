#pragma once

#include <functional>
#include <vector>

#include "log/LogEntry.h"

/**
 * @brief Abstraction the UI depends on to view engine log entries at runtime (DIP boundary),
 *  mirroring IInspectionService's read/subscribe-only shape - Qt is strictly a viewer here,
 *  there are no mutation methods.
 */
class ILogService
{
public:
  virtual ~ILogService() = default;

  virtual std::vector<LogEntry> GetEntries() const = 0; // oldest-first snapshot, bounded by retention cap

  using EntriesChangedCallback = std::function<void()>;
  // Registers a callback invoked whenever new entries are appended (push-based).
  virtual void OnEntriesChanged(EntriesChangedCallback callback) = 0;
};
