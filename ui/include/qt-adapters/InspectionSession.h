#pragma once

#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Inspection/IInspectionService.h"
#include "qt-adapters/InspectObjectSummary.h"

/**
 * @brief Engine-agnostic selection/query logic for the inspection UI. Holds no Qt types,
 *  so it is usable/testable independently of QTSceneInspector. Operates purely against
 *  the IInspectionService abstraction (never touches Scene/engine internals directly).
 */
class InspectionSession
{
public:
  void SetService(IInspectionService* servicePtr);
  IInspectionService* Service() const { return service; }

  std::vector<InspectObjectSummary> ProviderSummaries() const;

  const std::string& SelectedId() const { return selectedId; }
  bool HasSelection() const { return !selectedId.empty(); }

  // Selects by stable id; returns true if a matching provider was found.
  bool SelectById(const std::string& id);
  // Casts a ray and selects the closest hit provider; returns true if something was hit.
  bool SelectByRay(const glm::vec3& origin, const glm::vec3& direction);
  void ClearSelection();

  std::vector<InspectFieldPtr> CurrentFields() const;

  bool SupportsVisibility(const std::string& id) const;
  bool IsVisible(const std::string& id) const;
  bool SetVisible(const std::string& id, bool visible);

private:
  IInspectionService* service = nullptr;
  std::string selectedId;
};
