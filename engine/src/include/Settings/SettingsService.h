#pragma once

#include <cstdint>

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

  // Not part of ISettingsService: called once per Engine::Update() tick to detect
  // registration/unregistration (via EngineSettings' generation counter) and fire the
  // push notification. Mirrors EngineLogService::PollForEntryChanges().
  void PollForSettingsChanges();

private:
  std::uint64_t lastSeenGeneration = 0;
  SettingsChangedCallback settingsChangedCallback;
};
