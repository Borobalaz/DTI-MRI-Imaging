#pragma once

#include "Gui/panels/PanelRegistry.h"
#include "Gui/SceneFramebuffer.h"

class Scene;
class BaseMovement;

class GuiRoot
{
public:
  GuiRoot();

  void Render(Scene& scene,
              float deltaTime,
              int windowFramebufferWidth,
              int windowFramebufferHeight,
              BaseMovement* movement);

  void Destroy();

private:
  PanelRegistry panelRegistry;
  SceneFramebuffer sceneFramebuffer;
};
