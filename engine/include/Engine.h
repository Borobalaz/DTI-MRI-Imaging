#pragma once

#include <memory>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Inspection/InspectField.h"
#include "Inspection/IInspectionService.h"

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

  // Picking
  Ray ScreenPointToRay(float ndcX, float ndcY) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
