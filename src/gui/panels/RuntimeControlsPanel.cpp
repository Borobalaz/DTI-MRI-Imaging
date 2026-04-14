#include "Gui/panels/RuntimeControlsPanel.h"

#include <cfloat>
#include <memory>
#include <vector>

#include <imgui.h>

#include "Gui/widgets/FieldWidgets.h"
#include "Gui/widgets/IWidget.h"
#include "Scene/Scene.h"

RuntimeControlsPanel::RuntimeControlsPanel()
{
  RegisterWidget(UiFieldKind::Bool, std::make_unique<BoolFieldWidget>());
  RegisterWidget(UiFieldKind::Int, std::make_unique<IntFieldWidget>());
  RegisterWidget(UiFieldKind::Float, std::make_unique<FloatFieldWidget>());
  RegisterWidget(UiFieldKind::Vec3, std::make_unique<Vec3FieldWidget>());
  RegisterWidget(UiFieldKind::Color3, std::make_unique<Color3FieldWidget>());
  RegisterWidget(UiFieldKind::ComboBox, std::make_unique<ComboBoxFieldWidget>());
}

void RuntimeControlsPanel::RegisterWidget(UiFieldKind kind, std::unique_ptr<IWidget> widget)
{
  if (!widget)
  {
    return;
  }

  widgets[kind] = std::move(widget);
}

const IWidget* RuntimeControlsPanel::FindWidget(UiFieldKind kind) const
{
  const auto it = widgets.find(kind);
  return (it != widgets.end()) ? it->second.get() : nullptr;
}

void RuntimeControlsPanel::Render(GuiRenderContext& context)
{
  ImGui::Begin("Runtime Controls");

  ImGui::Separator();
  RenderInspectableControls(context.scene);
  RenderFps(context.deltaTime);

  ImGui::End();
}

bool RuntimeControlsPanel::HasRenderableContent(const InspectableNode& node) const
{
  if (node.isField)
  {
    return node.field.getter && node.field.setter && (FindWidget(node.field.kind) != nullptr);
  }

  if (!node.nestedInspectable)
  {
    return false;
  }

  std::vector<InspectableNode> nestedNodes;
  node.nestedInspectable->CollectInspectableNodes(nestedNodes, "");
  for (const InspectableNode& nestedNode : nestedNodes)
  {
    if (HasRenderableContent(nestedNode))
    {
      return true;
    }
  }

  return false;
}

void RuntimeControlsPanel::RenderInspectableNode(const InspectableNode& node, int depth) const
{
  (void)depth;

  if (node.isField)
  {
    const UiField& field = node.field;
    if (!field.getter || !field.setter)
    {
      return;
    }

    const IWidget* widget = FindWidget(field.kind);
    if (widget == nullptr)
    {
      return;
    }

    UiFieldValue value = field.getter();

    ImGui::PushID(node.nodeLabel.c_str());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(field.label.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PushItemWidth(-FLT_MIN);

    if (widget->Render(field, value))
    {
      field.setter(value);
    }

    ImGui::PopItemWidth();
    ImGui::PopID();
    return;
  }

  if (!node.nestedInspectable)
  {
    return;
  }

  std::vector<InspectableNode> nestedNodes;
  node.nestedInspectable->CollectInspectableNodes(nestedNodes, "");

  std::vector<const InspectableNode*> visibleNestedNodes;
  visibleNestedNodes.reserve(nestedNodes.size());
  for (const InspectableNode& nestedNode : nestedNodes)
  {
    if (HasRenderableContent(nestedNode))
    {
      visibleNestedNodes.push_back(&nestedNode);
    }
  }

  if (visibleNestedNodes.empty())
  {
    return;
  }

  const GuiTheme::NodeHeaderStyle& style = theme.GetNodeHeaderStyle();
  ImGui::PushStyleColor(ImGuiCol_Header, style.normal);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.hovered);
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.active);

  ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed;
  if (ImGui::TreeNodeEx(node.nodeLabel.c_str(), treeFlags))
  {
    ImGui::Indent();

    for (const InspectableNode* nestedNode : visibleNestedNodes)
    {
      RenderInspectableNode(*nestedNode, depth + 1);
    }

    ImGui::Unindent();
    ImGui::TreePop();
  }

  ImGui::PopStyleColor(3);
}

void RuntimeControlsPanel::RenderInspectableControls(Scene& scene)
{
  std::vector<InspectableNode> nodes;
  scene.CollectInspectableNodes(nodes);

  for (const InspectableNode& node : nodes)
  {
    if (!HasRenderableContent(node))
    {
      continue;
    }

    RenderInspectableNode(node, 0);
  }
}

void RuntimeControlsPanel::RenderFps(float deltaTime) const
{
  const float fps = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
  ImGui::Text("FPS: %.1f", fps);
}
