#include "Gui/panels/PanelRegistry.h"

void PanelRegistry::RegisterPanel(std::unique_ptr<GUIPanel> panel)
{
  if (!panel)
  {
    return;
  }

  panels.push_back(std::move(panel));
}

bool PanelRegistry::IsEmpty() const
{
  return panels.empty();
}

void PanelRegistry::RenderAll(GuiRenderContext& context)
{
  for (const std::unique_ptr<GUIPanel>& panel : panels)
  {
    panel->Render(context);
  }
}
