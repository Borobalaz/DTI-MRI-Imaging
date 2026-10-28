#pragma once

#include <cstdint>
#include <mutex>

#include "inspection/IInspectionService.h"

class Scene;
class SceneInspectionService;

/**
 * @brief IInspectionService decorator that makes the scene's inspection service safe to use from
 *  the host thread while the engine thread ticks: every call (and every getter/setter of the
 *  fields it returns) runs under the engine's scene mutex.
 *
 *  Field closures capture raw provider pointers, so a field fetched before its provider was
 *  removed must never run again. GetFields therefore returns wrapped fields stamped with the
 *  scene's provider generation (plus an attach epoch, so a re-created scene can't alias an old
 *  generation); once that changes, the wrapped getter returns the field's initialValue and the
 *  setter does nothing. The host refetches fields from the providers-changed notification.
 *
 *  Long-lived (owned by the engine runtime for the Engine's lifetime); the scene's service is
 *  attached/detached as the scene is created/destroyed. While detached every query returns an
 *  empty result. Returned fields must not outlive the Engine.
 */
class LockedInspectionService : public IInspectionService
{
public:
  explicit LockedInspectionService(std::recursive_mutex& sceneMutex);

  // Both require the scene mutex to be held by the caller (the engine runtime).
  void Attach(SceneInspectionService& service, const Scene& scene);
  void Detach();

  std::vector<InspectHandle> GetProviders() const override;
  std::optional<InspectHandle> FindProviderById(const std::string& id) const override;
  std::optional<InspectHandle> PickProviderByRay(const glm::vec3& origin, const glm::vec3& direction) const override;

  std::string GetDisplayName(const InspectHandle& handle) const override;
  std::vector<InspectFieldPtr> GetFields(const InspectHandle& handle) const override;

  bool SupportsVisibility(const InspectHandle& handle) const override;
  bool IsVisible(const InspectHandle& handle) const override;
  void SetVisible(const InspectHandle& handle, bool visible) override;

  // Stored here and forwarded to whichever scene service is (or later gets) attached.
  void OnProvidersChanged(ProvidersChangedCallback callback) override;

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
  SceneInspectionService* inner = nullptr; // guarded by sceneMutex
  const Scene* scene = nullptr;            // guarded by sceneMutex
  std::uint64_t attachEpoch = 0;           // guarded by sceneMutex
  ProvidersChangedCallback providersChangedCallback; // guarded by sceneMutex
};
