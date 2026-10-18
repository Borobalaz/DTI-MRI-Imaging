#include "Settings/SettingsService.h"

#include "Settings/EngineSettings.h"

std::vector<InspectFieldPtr> SettingsService::GetSettings() const
{
  return EngineSettings::Snapshot();
}

void SettingsService::OnSettingsChanged(SettingsChangedCallback callback)
{
  settingsChangedCallback = std::move(callback);
}

void SettingsService::PollForSettingsChanges()
{
  const std::uint64_t generation = EngineSettings::Generation();
  if (generation != lastSeenGeneration)
  {
    lastSeenGeneration = generation;
    if (settingsChangedCallback)
    {
      settingsChangedCallback();
    }
  }
}
