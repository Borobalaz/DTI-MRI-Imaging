#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

enum class InspectFieldType
{
  Action,
  Boolean,
  Color,
  Dropdown,
  File,
  Number,
  Text,
  Vector3
};

using InspectValue = std::variant<std::monostate, bool, int, double, std::string, glm::vec3>;

struct InspectField
{
  std::string id;
  std::string displayName;
  std::string groupName;
  InspectFieldType type = InspectFieldType::Text;
  bool readOnly = false;
  double minimum = 0.0;
  double maximum = 1.0;
  double step = 0.01;
  std::vector<std::string> enumOptions;
  InspectValue initialValue;
  std::function<InspectValue()> getter;
  std::function<void(const InspectValue&)> setter;

  InspectValue GetValue() const
  {
    return getter ? getter() : initialValue;
  }

  void SetValue(const InspectValue& value) const
  {
    if (setter && !readOnly)
    {
      setter(value);
    }
  }
};

using InspectFieldPtr = std::shared_ptr<InspectField>;

class InspectProvider
{
public:
  virtual ~InspectProvider() = default;

  virtual std::string GetInspectDisplayName() const = 0;
  virtual std::vector<InspectFieldPtr> GetInspectFields() = 0;
  virtual bool HasVisibility() const { return false; }
  virtual bool IsVisible() const { return true; }
  virtual void SetVisible(bool visible) { (void)visible; } // no-op unless HasVisibility() is overridden to return true
  virtual std::optional<float> CastRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const
  {
    (void)rayOrigin;
    (void)rayDirection;
    return std::nullopt;
  }
};

inline InspectFieldPtr MakeInspectField(std::string id,
                                        std::string displayName,
                                        std::string groupName,
                                        InspectFieldType type,
                                        InspectValue initialValue,
                                        std::function<InspectValue()> getter = {},
                                        std::function<void(const InspectValue&)> setter = {})
{
  auto field = std::make_shared<InspectField>();
  field->id = std::move(id);
  field->displayName = std::move(displayName);
  field->groupName = std::move(groupName);
  field->type = type;
  field->initialValue = std::move(initialValue);
  field->getter = std::move(getter);
  field->setter = std::move(setter);
  return field;
}
