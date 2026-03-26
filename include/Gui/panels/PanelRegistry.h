#pragma once

#include <memory>
#include <vector>

#include "Gui/panels/GUIPanel.h"

class PanelRegistry
{
public:
  void RegisterPanel(std::unique_ptr<GUIPanel> panel);
  bool IsEmpty() const;
  void RenderAll(GuiRenderContext& context);

private:
  std::vector<std::unique_ptr<GUIPanel>> panels;
};
