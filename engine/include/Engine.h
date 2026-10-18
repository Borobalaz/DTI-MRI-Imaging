#pragma once

#include <memory>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Inspection/InspectField.h"
#include "Inspection/IInspectionService.h"
#include "Log/ILogService.h"
#include "Settings/ISettingsService.h"

class Scene;
class ForwardRenderer;
class ExtractionRegistry;
class RenderFrameBuilder;
class InputState;
class SceneInspectionService;
class EngineLogService;
class SettingsService;

/**
 * @brief Public façade for the engine library. This is the only class UI code
 *  should depend on; all other engine headers are private implementation details.
 */
class Engine
{
public:
  struct Ray
  {
    glm::vec3 origin;
    glm::vec3 direction;
  };

  Engine();
  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // Lifecycle
  bool InitializeOpenGL(GLADloadproc loadProc);
  void CreateScene();

  // Per-frame
  void SetViewportSize(int width, int height);
  void SetFillColor(const glm::vec3& color);
  void Update(float deltaSeconds);
  void Render();

  // Input
  void OnKeyChanged(int key, bool down);
  bool IsKeyDown(int key) const;
  void OnMouseButtonChanged(int button, bool down);
  void OnMousePositionChanged(const glm::vec2& position);
  void OnScroll(float delta);

  // Inspection
  IInspectionService& GetInspectionService();

  // Log
  ILogService& GetLogService();

  // Settings
  ISettingsService& GetSettingsService();

  // Picking
  Ray ScreenPointToRay(float ndcX, float ndcY) const;

private:
  std::unique_ptr<Scene> scene;
  std::unique_ptr<ForwardRenderer> renderer;
  std::unique_ptr<ExtractionRegistry> extractionRegistry;
  std::unique_ptr<RenderFrameBuilder> renderFrameBuilder;
  std::unique_ptr<InputState> inputState;
  std::unique_ptr<SceneInspectionService> inspectionService;
  std::unique_ptr<EngineLogService> logService;
  std::unique_ptr<SettingsService> settingsService;
};
