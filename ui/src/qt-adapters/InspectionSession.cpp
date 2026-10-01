#include "qt-adapters/InspectionSession.h"

void InspectionSession::SetService(IInspectionService* servicePtr)
{
  service = servicePtr;
  selectedId.clear();

  if (service)
  {
    const std::vector<InspectHandle> providers = service->GetProviders();
    if (!providers.empty())
    {
      selectedId = providers.front().GetId();
    }
  }
}

std::vector<InspectObjectSummary> InspectionSession::ProviderSummaries() const
{
  std::vector<InspectObjectSummary> summaries;
  if (!service)
  {
    return summaries;
  }

  const std::vector<InspectHandle> providers = service->GetProviders();
  summaries.reserve(providers.size());
  for (const InspectHandle& handle : providers)
  {
    InspectObjectSummary summary;
    summary.id = handle.GetId();
    summary.displayName = service->GetDisplayName(handle);
    summary.hasVisibility = service->SupportsVisibility(handle);
    summary.isVisible = summary.hasVisibility && service->IsVisible(handle);
    summaries.push_back(std::move(summary));
  }
  return summaries;
}

bool InspectionSession::SelectById(const std::string& id)
{
  if (!service)
  {
    return false;
  }

  const std::optional<InspectHandle> handle = service->FindProviderById(id);
  if (!handle.has_value())
  {
    selectedId.clear();
    return false;
  }

  selectedId = id;
  return true;
}

bool InspectionSession::SelectByRay(const glm::vec3& origin, const glm::vec3& direction)
{
  if (!service)
  {
    return false;
  }

  const std::optional<InspectHandle> hit = service->PickProviderByRay(origin, direction);
  if (!hit.has_value())
  {
    return false;
  }

  selectedId = hit->GetId();
  return true;
}

void InspectionSession::ClearSelection()
{
  selectedId.clear();
}

std::vector<InspectFieldPtr> InspectionSession::CurrentFields() const
{
  if (!service || selectedId.empty())
  {
    return {};
  }

  const std::optional<InspectHandle> handle = service->FindProviderById(selectedId);
  if (!handle.has_value())
  {
    return {};
  }

  return service->GetFields(*handle);
}

bool InspectionSession::SupportsVisibility(const std::string& id) const
{
  if (!service)
  {
    return false;
  }

  const std::optional<InspectHandle> handle = service->FindProviderById(id);
  return handle.has_value() && service->SupportsVisibility(*handle);
}

bool InspectionSession::IsVisible(const std::string& id) const
{
  if (!service)
  {
    return false;
  }

  const std::optional<InspectHandle> handle = service->FindProviderById(id);
  return handle.has_value() && service->IsVisible(*handle);
}

bool InspectionSession::SetVisible(const std::string& id, bool visible)
{
  if (!service)
  {
    return false;
  }

  const std::optional<InspectHandle> handle = service->FindProviderById(id);
  if (!handle.has_value() || !service->SupportsVisibility(*handle))
  {
    return false;
  }

  service->SetVisible(*handle, visible);
  return true;
}
