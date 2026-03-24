#pragma once

#include <cstdint>
#include <string>

#include <glad/glad.h>
#include <imgui.h>

class Scene;
class InspectionMovement;

class UIRenderer
{
public:
  struct SceneFramebuffer
  {
    GLuint framebuffer = 0;
    GLuint colorTexture = 0;
    GLuint depthStencilRenderbuffer = 0;
    int width = 0;
    int height = 0;

    void EnsureSize(int newWidth, int newHeight);
    void Destroy();
    ImTextureID GetImGuiTextureId() const;
  };

  static void RenderRuntimeControls(Scene& scene, float deltaTime);
  static void RenderScenePanel(Scene& scene,
                               SceneFramebuffer& sceneFramebuffer,
                               int windowFramebufferWidth,
                               int windowFramebufferHeight,
                               float deltaTime,
                               InspectionMovement* inspectionMovement);

private:
  static std::string PickVolumeFilePath();
  static void HandleVolumeLoadAction(Scene& scene, std::string& volumeLoadStatus);
  static std::string TryLoadVolumeStatus(Scene& scene, const std::string& filePath);
  static void RenderVolumeLoadStatus(const std::string& volumeLoadStatus);
  static void RenderInspectableControls(Scene& scene);
  static void RenderInspectableNode(const class InspectableNode& node, int depth = 0);
  static void RenderFps(float deltaTime);
};
