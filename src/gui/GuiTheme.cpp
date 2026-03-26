#include "Gui/GuiTheme.h"

const GuiTheme::NodeHeaderStyle& GuiTheme::GetNodeHeaderStyle() const
{
  return nodeHeaderStyle;
}

void GuiTheme::SetNodeHeaderStyle(const NodeHeaderStyle& style)
{
  nodeHeaderStyle = style;
}
