#include "Log/EngineLogService.h"

#include "Log/EngineLog.h"

std::vector<LogEntry> EngineLogService::GetEntries() const
{
  return EngineLog::Snapshot();
}

void EngineLogService::OnEntriesChanged(EntriesChangedCallback callback)
{
  entriesChangedCallback = std::move(callback);
}

void EngineLogService::PollForEntryChanges()
{
  const std::uint64_t generation = EngineLog::Generation();
  if (generation != lastSeenGeneration)
  {
    lastSeenGeneration = generation;
    if (entriesChangedCallback)
    {
      entriesChangedCallback();
    }
  }
}
