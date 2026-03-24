#pragma once

#include <functional>
#include <memory>
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

class IInspectable;

/**
 * @brief Represents a hierarchical node in the inspectable tree.
 * Can be either a field or a nested inspectable object.
 */
struct InspectableNode
{
  std::string nodeLabel;  // Display name for this node
  
  // Either contains a field (if isField=true) or a nested IInspectable (if isField=false)
  bool isField = false;
  UiField field;  // Valid when isField=true
  std::shared_ptr<IInspectable> nestedInspectable;  // Valid when isField=false
};

class IInspectable
{
public:
  virtual ~IInspectable() = default;
  virtual void CollectInspectableFields(std::vector<UiField>& out, const std::string& groupPrefix) = 0;
  
  /**
   * @brief Collect hierarchical inspectable nodes (fields and nested objects).
   * Default implementation calls CollectInspectableFields and creates field nodes.
   * Override to add nested InspectableNode objects.
   */
  virtual void CollectInspectableNodes(std::vector<InspectableNode>& out, const std::string& nodePrefix)
  {
    std::vector<UiField> fields;
    CollectInspectableFields(fields, nodePrefix);
    
    for (const UiField& field : fields)
    {
      InspectableNode node;
      node.nodeLabel = field.label;
      node.isField = true;
      node.field = field;
      out.push_back(node);
    }
  }
};
