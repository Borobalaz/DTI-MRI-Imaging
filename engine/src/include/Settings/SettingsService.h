#pragma once

#include <cstdint>
#include <mutex>

#include "Settings/ISettingsService.h"

/**
 * @brief Concrete ISettingsService backed by the EngineSettings static registry. Internal
 *  engine detail; only reachable through Engine::GetSettingsService(). Scene-independent --
 *  constructed once in Engine's constructor, not in Engine::CreateScene(), same as
 *  EngineLogService.
 */
class SettingsService : public ISettingsService
{
public:
  std::vector<InspectFieldPtr> GetSettings() const override;
  void OnSettingsChanged(SettingsChangedCallback callback) override;

  // Not part of ISettingsService: called once per engine tick to detect registration/
  // unregistration (via EngineSettings' generation counter). Returns true on a change; the
  // caller then calls NotifyChanged() outside any engine lock. Mirrors
  // EngineLogService::PollForEntryChanges().
  bool PollForSettingsChanges();

  // Invokes the registered callback (if any). Must not be called with an engine lock held.
  void NotifyChanged();

private:
  std::uint64_t lastSeenGeneration = 0; // engine thread only
  std::mutex callbackMutex; // guards settingsChangedCallback (registered from the host thread)
  SettingsChangedCallback settingsChangedCallback;
};
