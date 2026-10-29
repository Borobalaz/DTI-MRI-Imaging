#pragma once

#include <functional>
#include <vector>

#include "inspection/InspectField.h"

/**
 * @brief Abstraction the UI depends on to view/edit the active scene's scene-wide controls
 *  (preprocessing, tractography, rotation, ...) at runtime (DIP boundary), shaped like
 *  ISettingsService: a flat field list with no handle/provider indirection, because these fields
 *  belong to the scene itself rather than to a selectable scene object. Editing happens by calling
 *  InspectField::SetValue() directly on a field returned from GetFields() -- this interface only
 *  needs to report the *set* of fields and when that set changes structurally.
 */
class ISceneDashboardService
{
public:
  virtual ~ISceneDashboardService() = default;

  virtual std::vector<InspectFieldPtr> GetFields() const = 0; // snapshot of the scene-wide fields

  using FieldsChangedCallback = std::function<void()>;
  // Registers a callback invoked whenever the field *set* structurally changes (scene recreated,
  // scene provider generation bumped) -- NOT fired on every value edit.
  virtual void OnFieldsChanged(FieldsChangedCallback callback) = 0;
};
