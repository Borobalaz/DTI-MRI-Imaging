#include "runtime/dashboard/SceneDashboardService.h"

#include "scene/Scene.h"

SceneDashboardService::SceneDashboardService(Scene& sceneRef)
  : scene(sceneRef)
{
}

std::vector<InspectFieldPtr> SceneDashboardService::GetFields() const
{
  return scene.GetDashboardFields();
}

void SceneDashboardService::OnFieldsChanged(FieldsChangedCallback callback)
{
  std::lock_guard<std::mutex> lock(callbackMutex);
  fieldsChangedCallback = std::move(callback);
}

bool SceneDashboardService::PollForFieldChanges()
{
  const int generation = scene.GetProviderGeneration();
  if (generation == lastSeenGeneration)
  {
    return false;
  }

  lastSeenGeneration = generation;
  return true;
}

void SceneDashboardService::NotifyChanged()
{
  FieldsChangedCallback callback;
  {
    std::lock_guard<std::mutex> lock(callbackMutex);
    callback = fieldsChangedCallback;
  }

  if (callback)
  {
    callback();
  }
}
