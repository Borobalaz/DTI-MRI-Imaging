#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Inspection/InspectField.h"
#include "Inspection/InspectHandle.h"

/**
 * @brief Abstraction the UI depends on to inspect/edit engine objects at runtime (DIP boundary).
 *  All queries take/return InspectHandle values rather than raw provider pointers.
 */
class IInspectionService
{
public:
  virtual ~IInspectionService() = default;

  virtual std::vector<InspectHandle> GetProviders() const = 0;
  virtual std::optional<InspectHandle> FindProviderById(const std::string& id) const = 0;
  virtual std::optional<InspectHandle> PickProviderByRay(const glm::vec3& origin, const glm::vec3& direction) const = 0;

  virtual std::string GetDisplayName(const InspectHandle& handle) const = 0;
  virtual std::vector<InspectFieldPtr> GetFields(const InspectHandle& handle) const = 0;

  virtual bool SupportsVisibility(const InspectHandle& handle) const = 0;
  virtual bool IsVisible(const InspectHandle& handle) const = 0;
  virtual void SetVisible(const InspectHandle& handle, bool visible) = 0;

  using ProvidersChangedCallback = std::function<void()>;
  // Registers a callback invoked whenever the provider list structurally changes (push-based).
  virtual void OnProvidersChanged(ProvidersChangedCallback callback) = 0;
};
