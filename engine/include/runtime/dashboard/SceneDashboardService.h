#pragma once

#include <mutex>

#include "dashboard/ISceneDashboardService.h"

class Scene;

/**
 * @brief Concrete ISceneDashboardService backed by a Scene's GetDashboardFields(). Internal engine
 *  detail; only reachable through Engine::GetSceneDashboardService().
 */
class SceneDashboardService : public ISceneDashboardService
{
public:
  explicit SceneDashboardService(Scene& sceneRef);

  std::vector<InspectFieldPtr> GetFields() const override;

  void OnFieldsChanged(FieldsChangedCallback callback) override;

  // Not part of ISceneDashboardService: called once per engine tick, under the scene lock, to
  // detect field-set changes (via Scene's provider generation counter). Returns true on a change;
  // the caller then calls NotifyChanged() after releasing the scene lock.
  bool PollForFieldChanges();

  // Invokes the registered callback (if any). Must not be called with the scene lock held.
  void NotifyChanged();

private:
  Scene& scene;
  int lastSeenGeneration = -1;
  std::mutex callbackMutex; // guards fieldsChangedCallback (registered from the host thread)
  FieldsChangedCallback fieldsChangedCallback;
};
