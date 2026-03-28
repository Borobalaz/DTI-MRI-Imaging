#include "Gui/GuiTheme.h"

/**
 * @brief Gets the node header style including normal, hovered, and active colors
 * @return The node header style
 */
const GuiTheme::NodeHeaderStyle& GuiTheme::GetNodeHeaderStyle() const
{
  return nodeHeaderStyle;
}

/**
 * @brief Sets the node header style
 * @param style The node header style
 */
void GuiTheme::SetNodeHeaderStyle(const NodeHeaderStyle& style)
{
  nodeHeaderStyle = style;
}
