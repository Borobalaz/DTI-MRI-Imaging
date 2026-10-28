#pragma once

#include <mutex>

#include "settings/ISettingsService.h"

/**
 * @brief ISettingsService decorator that serializes every call -- and every getter/setter of the
 *  fields it returns -- with the engine tick by taking the engine's scene mutex, built the same
 *  way as LockedInspectionService. Settings declarations are permanent for the process, so
 *  unlike inspection fields the wrapped fields never go stale.
 *
 *  Returned fields must not outlive the Engine.
 */
class LockedSettingsService : public ISettingsService
{
public:
  LockedSettingsService(std::recursive_mutex& sceneMutex, ISettingsService& inner);

  std::vector<InspectFieldPtr> GetSettings() const override;
  void OnSettingsChanged(SettingsChangedCallback callback) override;

private:
  InspectFieldPtr WrapField(const InspectFieldPtr& field) const;

  std::recursive_mutex& sceneMutex;
  ISettingsService& inner;
};
