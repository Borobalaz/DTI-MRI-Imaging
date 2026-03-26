#pragma once

#include <memory>
#include <unordered_map>

#include "Gui/panels/GUIPanel.h"
#include "Gui/GuiTheme.h"
#include "Gui/Inspectable.h"

class IWidget;

struct UiFieldKindHash
{
  std::size_t operator()(UiFieldKind kind) const noexcept
  {
    return static_cast<std::size_t>(kind);
  }
};

class RuntimeControlsPanel : public GUIPanel
{
public:
  RuntimeControlsPanel();
  void Render(GuiRenderContext& context) override;

private:
  void RegisterWidget(UiFieldKind kind, std::unique_ptr<IWidget> widget);
  const IWidget* FindWidget(UiFieldKind kind) const;

  bool HasRenderableContent(const InspectableNode& node) const;
  void RenderInspectableNode(const InspectableNode& node, int depth = 0) const;

  void RenderInspectableControls(Scene& scene);
  void RenderFps(float deltaTime) const;

  GuiTheme theme;
  std::unordered_map<UiFieldKind, std::unique_ptr<IWidget>, UiFieldKindHash> widgets;
};
