#include "Log/EngineLogService.h"

#include "Log/EngineLog.h"

std::vector<LogEntry> EngineLogService::GetEntries() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return EngineLog::Snapshot();
}

void EngineLogService::OnEntriesChanged(EntriesChangedCallback callback)
{
  std::lock_guard<std::mutex> lock(mutex);
  entriesChangedCallback = std::move(callback);
}

bool EngineLogService::PollForEntryChanges()
{
  const std::uint64_t generation = EngineLog::Generation();

  std::lock_guard<std::mutex> lock(mutex);
  if (generation == lastSeenGeneration)
  {
    return false;
  }

  lastSeenGeneration = generation;
  return true;
}

void EngineLogService::NotifyChanged()
{
  EntriesChangedCallback callback;
  {
    std::lock_guard<std::mutex> lock(mutex);
    callback = entriesChangedCallback;
  }

  if (callback)
  {
    callback();
  }
}
