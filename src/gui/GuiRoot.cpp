#include "Gui/GuiRoot.h"

#include <memory>

#include "BaseMovement.h"
#include "Gui/GuiRenderContext.h"
#include "Gui/panels/RuntimeControlsPanel.h"
#include "Gui/panels/ScenePanel.h"
#include "Scene/Scene.h"

/**
 * @brief Constructs a new GuiRoot instance, adds panels to the registry
 */
GuiRoot::GuiRoot()
{
  panelRegistry.RegisterPanel(std::make_unique<RuntimeControlsPanel>());
  panelRegistry.RegisterPanel(std::make_unique<ScenePanel>());
}

/**
 * @brief Renders the GUI
 * 
 * @param scene 
 * @param deltaTime 
 * @param windowFramebufferWidth 
 * @param windowFramebufferHeight 
 * @param movement - The movement component on the scene's camera
 */
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

/**
 * @brief Destroys the GUI root and its associated resources
 * 
 */
void GuiRoot::Destroy()
{
  sceneFramebuffer.Destroy();
}
