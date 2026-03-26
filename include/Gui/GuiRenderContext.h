#pragma once

class Scene;
class BaseMovement;
class SceneFramebuffer;

struct GuiRenderContext
{
  Scene& scene;
  float deltaTime;

  SceneFramebuffer* sceneFramebuffer = nullptr;
  int windowFramebufferWidth = 0;
  int windowFramebufferHeight = 0;
  BaseMovement* movement = nullptr;
};
