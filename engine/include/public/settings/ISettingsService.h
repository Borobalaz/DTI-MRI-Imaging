#pragma once

#include <functional>
#include <vector>

#include "inspection/InspectField.h"

/**
 * @brief Abstraction the UI depends on to view/edit flat, non-selection-based engine-wide
 *  settings at runtime (DIP boundary), mirroring ILogService's read/subscribe shape but with
 *  mutable fields: unlike IInspectionService there is no handle/provider indirection because
 *  settings aren't scoped to a selectable scene object. Editing happens by calling
 *  InspectField::SetValue() directly on a field returned from GetSettings() -- this interface
 *  only needs to report the *set* of registered settings and when that set changes structurally.
 */
class ISettingsService
{
public:
  virtual ~ISettingsService() = default;

  virtual std::vector<InspectFieldPtr> GetSettings() const = 0; // snapshot of all currently registered settings

  using SettingsChangedCallback = std::function<void()>;
  // Registers a callback invoked whenever the registered-settings set structurally changes
  // (a setting registered/unregistered) -- NOT fired on every value edit.
  virtual void OnSettingsChanged(SettingsChangedCallback callback) = 0;
};
