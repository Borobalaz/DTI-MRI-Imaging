#include "Settings/SettingsService.h"

#include "Settings/EngineSettings.h"

std::vector<InspectFieldPtr> SettingsService::GetSettings() const
{
  return EngineSettings::Snapshot();
}

void SettingsService::OnSettingsChanged(SettingsChangedCallback callback)
{
  std::lock_guard<std::mutex> lock(callbackMutex);
  settingsChangedCallback = std::move(callback);
}

bool SettingsService::PollForSettingsChanges()
{
  const std::uint64_t generation = EngineSettings::Generation();
  if (generation == lastSeenGeneration)
  {
    return false;
  }

  lastSeenGeneration = generation;
  return true;
}

void SettingsService::NotifyChanged()
{
  SettingsChangedCallback callback;
  {
    std::lock_guard<std::mutex> lock(callbackMutex);
    callback = settingsChangedCallback;
  }

  if (callback)
  {
    callback();
  }
}
