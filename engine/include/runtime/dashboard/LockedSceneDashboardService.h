#pragma once

#include <cstdint>
#include <mutex>

#include "dashboard/ISceneDashboardService.h"

class Scene;
class SceneDashboardService;

/**
 * @brief ISceneDashboardService decorator that makes the scene's dashboard service safe to use
 *  from the host thread while the engine thread ticks: every call (and every getter/setter of the
 *  fields it returns) runs under the engine's scene mutex.
 *
 *  Dashboard field closures capture the scene's `this` (and objects it may later drop), so a field
 *  fetched before a structural change must never run again. GetFields therefore returns wrapped
 *  fields stamped with the scene's provider generation (plus an attach epoch, so a re-created scene
 *  can't alias an old generation); once that changes, the wrapped getter returns the field's
 *  initialValue and the setter does nothing. The host refetches fields from the fields-changed
 *  notification. Mirrors LockedInspectionService.
 *
 *  Long-lived (owned by the engine runtime for the Engine's lifetime); the scene's service is
 *  attached/detached as the scene is created/destroyed. While detached GetFields returns an empty
 *  list. Returned fields must not outlive the Engine.
 */
class LockedSceneDashboardService : public ISceneDashboardService
{
public:
  explicit LockedSceneDashboardService(std::recursive_mutex& sceneMutex);

  // Both require the scene mutex to be held by the caller (the engine runtime).
  void Attach(SceneDashboardService& service, const Scene& scene);
  void Detach();

  std::vector<InspectFieldPtr> GetFields() const override;

  // Stored here and forwarded to whichever scene service is (or later gets) attached.
  void OnFieldsChanged(FieldsChangedCallback callback) override;

private:
  struct FieldStamp
  {
    std::uint64_t attachEpoch = 0;
    int providerGeneration = 0;
  };

  // Caller holds sceneMutex.
  FieldStamp CurrentStampLocked() const;
  bool IsStampCurrentLocked(const FieldStamp& stamp) const;
  InspectFieldPtr WrapField(const InspectFieldPtr& field, const FieldStamp& stamp) const;

  std::recursive_mutex& sceneMutex;
  SceneDashboardService* inner = nullptr; // guarded by sceneMutex
  const Scene* scene = nullptr;           // guarded by sceneMutex
  std::uint64_t attachEpoch = 0;          // guarded by sceneMutex
  FieldsChangedCallback fieldsChangedCallback; // guarded by sceneMutex
};
