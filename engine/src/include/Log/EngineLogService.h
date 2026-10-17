#pragma once

#include <cstdint>

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

  // Not part of ILogService: called once per Engine::Update() tick to detect newly appended
  // entries (via EngineLog's generation counter) and fire the push notification.
  void PollForEntryChanges();

private:
  std::uint64_t lastSeenGeneration = 0;
  EntriesChangedCallback entriesChangedCallback;
};
