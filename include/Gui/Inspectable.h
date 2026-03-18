#pragma once

#include <functional>
#include <string>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

enum class UiFieldKind
{
  Bool,
  Int,
  Float,
  Vec3,
  Color3
};

using UiFieldValue = std::variant<bool, int, float, glm::vec3>;

struct UiField
{
  std::string group;
  std::string label;
  UiFieldKind kind = UiFieldKind::Float;

  float minFloat = 0.0f;
  float maxFloat = 1.0f;
  float speed = 0.01f;

  int minInt = 0;
  int maxInt = 100;

  std::function<UiFieldValue()> getter;
  std::function<void(const UiFieldValue&)> setter;
};

class IInspectable
{
public:
  virtual ~IInspectable() = default;
  virtual void CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix) = 0;
};
