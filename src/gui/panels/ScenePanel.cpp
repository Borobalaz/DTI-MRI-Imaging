#include "Gui/panels/ScenePanel.h"

#include <imgui.h>

#include "BaseMovement.h"
#include "Gui/SceneFramebuffer.h"
#include "InspectionMovement.h"
#include "Scene/Scene.h"

void ScenePanel::Render(GuiRenderContext& context)
{
  if (context.sceneFramebuffer == nullptr)
  {
    return;
  }

  Scene& scene = context.scene;
  SceneFramebuffer& sceneFramebuffer = *context.sceneFramebuffer;

  ImGui::Begin("Scene");

  if (context.movement != nullptr)
  {
    InspectionMovement* inspectionMovement = dynamic_cast<InspectionMovement*>(context.movement);
    if (inspectionMovement != nullptr)
    {
      const bool scenePanelFocused = ImGui::IsWindowFocused();
      inspectionMovement->SetInputEnabled(scenePanelFocused);
    }
  }

  const ImVec2 availableRegion = ImGui::GetContentRegionAvail();
  const int targetSceneWidth = (availableRegion.x >= 1.0f) ? static_cast<int>(availableRegion.x) : 1;
  const int targetSceneHeight = (availableRegion.y >= 1.0f) ? static_cast<int>(availableRegion.y) : 1;

  sceneFramebuffer.EnsureSize(targetSceneWidth, targetSceneHeight);
  scene.Update(context.deltaTime);

  glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer.framebuffer);
  glViewport(0, 0, sceneFramebuffer.width, sceneFramebuffer.height);
  scene.SetCameraAspect(static_cast<float>(sceneFramebuffer.width) / static_cast<float>(sceneFramebuffer.height));
  scene.Render();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, context.windowFramebufferWidth, context.windowFramebufferHeight);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGui::Image(
    sceneFramebuffer.GetImGuiTextureId(),
    ImVec2(static_cast<float>(sceneFramebuffer.width), static_cast<float>(sceneFramebuffer.height)),
    ImVec2(0.0f, 1.0f),
    ImVec2(1.0f, 0.0f)
  );

  ImGui::End();
}
