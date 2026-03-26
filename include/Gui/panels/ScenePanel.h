#pragma once

#include "Gui/panels/GUIPanel.h"

class ScenePanel : public GUIPanel
{
public:
  void Render(GuiRenderContext& context) override;
};
