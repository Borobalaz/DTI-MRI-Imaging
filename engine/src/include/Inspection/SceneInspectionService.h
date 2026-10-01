#pragma once

#include <limits>

#include "Inspection/IInspectionService.h"

class Scene;

/**
 * @brief Concrete IInspectionService backed by a Scene. Internal engine detail;
 *  only reachable through Engine::GetInspectionService().
 */
class SceneInspectionService : public IInspectionService
{
public:
  explicit SceneInspectionService(Scene& sceneRef);

  std::vector<InspectHandle> GetProviders() const override;
  std::optional<InspectHandle> FindProviderById(const std::string& id) const override;
  std::optional<InspectHandle> PickProviderByRay(const glm::vec3& origin, const glm::vec3& direction) const override;

  std::string GetDisplayName(const InspectHandle& handle) const override;
  std::vector<InspectFieldPtr> GetFields(const InspectHandle& handle) const override;

  bool SupportsVisibility(const InspectHandle& handle) const override;
  bool IsVisible(const InspectHandle& handle) const override;
  void SetVisible(const InspectHandle& handle, bool visible) override;

  void OnProvidersChanged(ProvidersChangedCallback callback) override;

  // Not part of IInspectionService: called once per Engine::Update() tick to detect
  // provider-list changes (via Scene's generation counter) and fire the push notification.
  void PollForProviderChanges();

private:
  Scene& scene;
  int lastSeenGeneration = -1;
  ProvidersChangedCallback providersChangedCallback;
};
