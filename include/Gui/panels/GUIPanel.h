#pragma once

#include "Gui/GuiRenderContext.h"

class GUIPanel
{
public:
  virtual ~GUIPanel() = default;
  virtual void Render(GuiRenderContext& context) = 0;
};
