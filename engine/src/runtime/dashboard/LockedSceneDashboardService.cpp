#include "runtime/dashboard/LockedSceneDashboardService.h"

#include "runtime/dashboard/SceneDashboardService.h"
#include "scene/Scene.h"

namespace
{
  using SceneLock = std::lock_guard<std::recursive_mutex>;
}

LockedSceneDashboardService::LockedSceneDashboardService(std::recursive_mutex& sceneMutexRef)
  : sceneMutex(sceneMutexRef)
{
}

void LockedSceneDashboardService::Attach(SceneDashboardService& service, const Scene& sceneRef)
{
  inner = &service;
  scene = &sceneRef;
  ++attachEpoch;

  if (fieldsChangedCallback)
  {
    inner->OnFieldsChanged(fieldsChangedCallback);
  }
}

void LockedSceneDashboardService::Detach()
{
  inner = nullptr;
  scene = nullptr;
  ++attachEpoch;
}

std::vector<InspectFieldPtr> LockedSceneDashboardService::GetFields() const
{
  SceneLock lock(sceneMutex);
  if (!inner)
  {
    return {};
  }

  const FieldStamp stamp = CurrentStampLocked();
  std::vector<InspectFieldPtr> wrappedFields;
  for (const InspectFieldPtr& field : inner->GetFields())
  {
    if (field)
    {
      wrappedFields.push_back(WrapField(field, stamp));
    }
  }
  return wrappedFields;
}

void LockedSceneDashboardService::OnFieldsChanged(FieldsChangedCallback callback)
{
  SceneLock lock(sceneMutex);
  fieldsChangedCallback = std::move(callback);
  if (inner)
  {
    inner->OnFieldsChanged(fieldsChangedCallback);
  }
}

LockedSceneDashboardService::FieldStamp LockedSceneDashboardService::CurrentStampLocked() const
{
  FieldStamp stamp;
  stamp.attachEpoch = attachEpoch;
  stamp.providerGeneration = scene ? scene->GetProviderGeneration() : 0;
  return stamp;
}

bool LockedSceneDashboardService::IsStampCurrentLocked(const FieldStamp& stamp) const
{
  const FieldStamp current = CurrentStampLocked();
  return scene && current.attachEpoch == stamp.attachEpoch
      && current.providerGeneration == stamp.providerGeneration;
}

InspectFieldPtr LockedSceneDashboardService::WrapField(const InspectFieldPtr& field, const FieldStamp& stamp) const
{
  // Copies the metadata (id, type, range, options, initialValue, readOnly); only the
  // accessors are replaced. The original field is kept alive by the wrapper's closures.
  auto wrapped = std::make_shared<InspectField>(*field);
  const InspectFieldPtr original = field;

  wrapped->getter = [this, original, stamp]() -> InspectValue
  {
    SceneLock lock(sceneMutex);
    return IsStampCurrentLocked(stamp) ? original->GetValue() : original->initialValue;
  };

  wrapped->setter = [this, original, stamp](const InspectValue& value)
  {
    SceneLock lock(sceneMutex);
    if (IsStampCurrentLocked(stamp))
    {
      original->SetValue(value);
    }
  };

  return wrapped;
}
