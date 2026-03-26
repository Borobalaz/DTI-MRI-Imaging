#pragma once

#include <imgui.h>

class GuiTheme
{
public:
  struct NodeHeaderStyle
  {
    ImVec4 normal = ImVec4(0.18f, 0.22f, 0.30f, 0.90f);
    ImVec4 hovered = ImVec4(0.24f, 0.30f, 0.40f, 0.95f);
    ImVec4 active = ImVec4(0.30f, 0.36f, 0.48f, 0.95f);
  };

  const NodeHeaderStyle& GetNodeHeaderStyle() const;
  void SetNodeHeaderStyle(const NodeHeaderStyle& style);

private:
  NodeHeaderStyle nodeHeaderStyle;
};
