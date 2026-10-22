#include "Inspection/LockedInspectionService.h"

#include "Inspection/SceneInspectionService.h"
#include "Scene/Scene.h"

namespace
{
  using SceneLock = std::lock_guard<std::recursive_mutex>;
}

LockedInspectionService::LockedInspectionService(std::recursive_mutex& sceneMutexRef)
  : sceneMutex(sceneMutexRef)
{
}

void LockedInspectionService::Attach(SceneInspectionService& service, const Scene& sceneRef)
{
  inner = &service;
  scene = &sceneRef;
  ++attachEpoch;

  if (providersChangedCallback)
  {
    inner->OnProvidersChanged(providersChangedCallback);
  }
}

void LockedInspectionService::Detach()
{
  inner = nullptr;
  scene = nullptr;
  ++attachEpoch;
}

std::vector<InspectHandle> LockedInspectionService::GetProviders() const
{
  SceneLock lock(sceneMutex);
  return inner ? inner->GetProviders() : std::vector<InspectHandle>{};
}

std::optional<InspectHandle> LockedInspectionService::FindProviderById(const std::string& id) const
{
  SceneLock lock(sceneMutex);
  return inner ? inner->FindProviderById(id) : std::nullopt;
}

std::optional<InspectHandle> LockedInspectionService::PickProviderByRay(const glm::vec3& origin, const glm::vec3& direction) const
{
  SceneLock lock(sceneMutex);
  return inner ? inner->PickProviderByRay(origin, direction) : std::nullopt;
}

std::string LockedInspectionService::GetDisplayName(const InspectHandle& handle) const
{
  SceneLock lock(sceneMutex);
  return inner ? inner->GetDisplayName(handle) : std::string();
}

std::vector<InspectFieldPtr> LockedInspectionService::GetFields(const InspectHandle& handle) const
{
  SceneLock lock(sceneMutex);
  if (!inner)
  {
    return {};
  }

  const FieldStamp stamp = CurrentStampLocked();
  std::vector<InspectFieldPtr> wrappedFields;
  for (const InspectFieldPtr& field : inner->GetFields(handle))
  {
    if (field)
    {
      wrappedFields.push_back(WrapField(field, stamp));
    }
  }
  return wrappedFields;
}

bool LockedInspectionService::SupportsVisibility(const InspectHandle& handle) const
{
  SceneLock lock(sceneMutex);
  return inner && inner->SupportsVisibility(handle);
}

bool LockedInspectionService::IsVisible(const InspectHandle& handle) const
{
  SceneLock lock(sceneMutex);
  return inner && inner->IsVisible(handle);
}

void LockedInspectionService::SetVisible(const InspectHandle& handle, bool visible)
{
  SceneLock lock(sceneMutex);
  if (inner)
  {
    inner->SetVisible(handle, visible);
  }
}

void LockedInspectionService::OnProvidersChanged(ProvidersChangedCallback callback)
{
  SceneLock lock(sceneMutex);
  providersChangedCallback = std::move(callback);
  if (inner)
  {
    inner->OnProvidersChanged(providersChangedCallback);
  }
}

LockedInspectionService::FieldStamp LockedInspectionService::CurrentStampLocked() const
{
  FieldStamp stamp;
  stamp.attachEpoch = attachEpoch;
  stamp.providerGeneration = scene ? scene->GetProviderGeneration() : 0;
  return stamp;
}

bool LockedInspectionService::IsStampCurrentLocked(const FieldStamp& stamp) const
{
  const FieldStamp current = CurrentStampLocked();
  return scene && current.attachEpoch == stamp.attachEpoch
      && current.providerGeneration == stamp.providerGeneration;
}

InspectFieldPtr LockedInspectionService::WrapField(const InspectFieldPtr& field, const FieldStamp& stamp) const
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
