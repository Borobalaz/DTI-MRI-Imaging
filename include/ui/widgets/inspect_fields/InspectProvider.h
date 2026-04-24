#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ui/widgets/inspect_fields/IInspectWidget.h"

class InspectProvider
{
public:
  virtual ~InspectProvider() = default;

  virtual std::string GetInspectDisplayName() const = 0;
  virtual std::vector<std::shared_ptr<IInspectWidget>> GetInspectFields() = 0;
  virtual bool HasVisibility() const { return false; }
  virtual bool IsVisible() const { return true; }
  virtual std::optional<float> CastRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const
  {
    return std::nullopt;
  }
};
