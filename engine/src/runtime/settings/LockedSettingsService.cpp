#include "runtime/settings/LockedSettingsService.h"

namespace
{
  using SceneLock = std::lock_guard<std::recursive_mutex>;
}

LockedSettingsService::LockedSettingsService(std::recursive_mutex& sceneMutexRef, ISettingsService& innerRef)
  : sceneMutex(sceneMutexRef),
    inner(innerRef)
{
}

std::vector<InspectFieldPtr> LockedSettingsService::GetSettings() const
{
  SceneLock lock(sceneMutex);

  std::vector<InspectFieldPtr> wrappedFields;
  for (const InspectFieldPtr& field : inner.GetSettings())
  {
    if (field)
    {
      wrappedFields.push_back(WrapField(field));
    }
  }
  return wrappedFields;
}

void LockedSettingsService::OnSettingsChanged(SettingsChangedCallback callback)
{
  SceneLock lock(sceneMutex);
  inner.OnSettingsChanged(std::move(callback));
}

InspectFieldPtr LockedSettingsService::WrapField(const InspectFieldPtr& field) const
{
  // Copies the metadata; only the accessors are replaced. The original field is kept alive
  // by the wrapper's closures.
  auto wrapped = std::make_shared<InspectField>(*field);
  const InspectFieldPtr original = field;

  wrapped->getter = [this, original]() -> InspectValue
  {
    SceneLock lock(sceneMutex);
    return original->GetValue();
  };

  wrapped->setter = [this, original](const InspectValue& value)
  {
    SceneLock lock(sceneMutex);
    original->SetValue(value);
  };

  return wrapped;
}
