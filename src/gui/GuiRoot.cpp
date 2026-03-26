#include "Gui/GuiRoot.h"

#include <memory>

#include "BaseMovement.h"
#include "Gui/GuiRenderContext.h"
#include "Gui/panels/RuntimeControlsPanel.h"
#include "Gui/panels/ScenePanel.h"
#include "Scene/Scene.h"

GuiRoot::GuiRoot()
{
  panelRegistry.RegisterPanel(std::make_unique<RuntimeControlsPanel>());
  panelRegistry.RegisterPanel(std::make_unique<ScenePanel>());
}

void GuiRoot::Render(Scene& scene,
                     float deltaTime,
                     int windowFramebufferWidth,
                     int windowFramebufferHeight,
                     BaseMovement* movement)
{
  GuiRenderContext context{scene, deltaTime};
  context.sceneFramebuffer = &sceneFramebuffer;
  context.windowFramebufferWidth = windowFramebufferWidth;
  context.windowFramebufferHeight = windowFramebufferHeight;
  context.movement = movement;

  panelRegistry.RenderAll(context);
}

void GuiRoot::Destroy()
{
  sceneFramebuffer.Destroy();
}
