#pragma once

#include <cstdint>
#include <mutex>

#include "Log/ILogService.h"

/**
 * @brief Concrete ILogService backed by the EngineLog static registry. Internal engine detail;
 *  only reachable through Engine::GetLogService(). Unlike SceneInspectionService, this is
 *  scene-independent - constructed once in Engine's constructor, not in Engine::CreateScene().
 */
class EngineLogService : public ILogService
{
public:
  std::vector<LogEntry> GetEntries() const override;
  void OnEntriesChanged(EntriesChangedCallback callback) override;

  // Not part of ILogService: called once per engine tick to detect newly appended entries
  // (via EngineLog's generation counter). Returns true when entries were appended; the caller
  // then calls NotifyChanged() outside any engine lock.
  bool PollForEntryChanges();

  // Invokes the registered callback (if any). Must not be called with an engine lock held.
  void NotifyChanged();

private:
  // GetEntries() runs on the host thread while workers append entries; the entries themselves
  // are guarded inside EngineLog, this guards the service's own state.
  mutable std::mutex mutex;
  std::uint64_t lastSeenGeneration = 0;
  EntriesChangedCallback entriesChangedCallback;
};
