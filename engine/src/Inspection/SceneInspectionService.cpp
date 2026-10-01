#include "Inspection/SceneInspectionService.h"

#include "Scene/Scene.h"

SceneInspectionService::SceneInspectionService(Scene& sceneRef)
  : scene(sceneRef)
{
}

std::vector<InspectHandle> SceneInspectionService::GetProviders() const
{
  std::vector<InspectHandle> handles;
  for (const auto& entry : scene.GetInspectProviderEntries())
  {
    if (!entry.provider.expired())
    {
      handles.emplace_back(entry.provider, entry.id);
    }
  }
  return handles;
}

std::optional<InspectHandle> SceneInspectionService::FindProviderById(const std::string& id) const
{
  for (const auto& entry : scene.GetInspectProviderEntries())
  {
    if (entry.id == id && !entry.provider.expired())
    {
      return InspectHandle(entry.provider, entry.id);
    }
  }
  return std::nullopt;
}

std::optional<InspectHandle> SceneInspectionService::PickProviderByRay(const glm::vec3& origin, const glm::vec3& direction) const
{
  const float directionLength = glm::length(direction);
  if (directionLength <= 1e-6f)
  {
    return std::nullopt;
  }

  const glm::vec3 normalizedDirection = direction / directionLength;
  float closestDistance = std::numeric_limits<float>::max();
  std::optional<InspectHandle> closest;

  for (const auto& entry : scene.GetInspectProviderEntries())
  {
    std::shared_ptr<InspectProvider> provider = entry.provider.lock();
    if (!provider)
    {
      continue;
    }

    const std::optional<float> hitDistance = provider->CastRay(origin, normalizedDirection);
    if (!hitDistance.has_value() || hitDistance.value() < 0.0f)
    {
      continue;
    }

    if (hitDistance.value() < closestDistance)
    {
      closestDistance = hitDistance.value();
      closest = InspectHandle(entry.provider, entry.id);
    }
  }

  return closest;
}

std::string SceneInspectionService::GetDisplayName(const InspectHandle& handle) const
{
  std::shared_ptr<InspectProvider> provider = handle.Lock();
  return provider ? provider->GetInspectDisplayName() : std::string();
}

std::vector<InspectFieldPtr> SceneInspectionService::GetFields(const InspectHandle& handle) const
{
  std::shared_ptr<InspectProvider> provider = handle.Lock();
  return provider ? provider->GetInspectFields() : std::vector<InspectFieldPtr>{};
}

bool SceneInspectionService::SupportsVisibility(const InspectHandle& handle) const
{
  std::shared_ptr<InspectProvider> provider = handle.Lock();
  return provider && provider->HasVisibility();
}

bool SceneInspectionService::IsVisible(const InspectHandle& handle) const
{
  std::shared_ptr<InspectProvider> provider = handle.Lock();
  return provider && provider->IsVisible();
}

void SceneInspectionService::SetVisible(const InspectHandle& handle, bool visible)
{
  if (std::shared_ptr<InspectProvider> provider = handle.Lock())
  {
    provider->SetVisible(visible);
  }
}

void SceneInspectionService::OnProvidersChanged(ProvidersChangedCallback callback)
{
  providersChangedCallback = std::move(callback);
}

void SceneInspectionService::PollForProviderChanges()
{
  const int generation = scene.GetProviderGeneration();
  if (generation != lastSeenGeneration)
  {
    lastSeenGeneration = generation;
    if (providersChangedCallback)
    {
      providersChangedCallback();
    }
  }
}
