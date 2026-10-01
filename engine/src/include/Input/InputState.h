#pragma once

#include <unordered_set>

#include <glm/vec2.hpp>

class InputState
{
public:
  void SetKeyDown(int key, bool down)
  {
    if (down)
    {
      keysDown.insert(key);
    }
    else
    {
      keysDown.erase(key);
    }
  }

  bool IsKeyDown(int key) const
  {
    return keysDown.find(key) != keysDown.end();
  }

  void SetMouseButtonDown(int button, bool down)
  {
    if (down)
    {
      mouseButtonsDown.insert(button);
    }
    else
    {
      mouseButtonsDown.erase(button);
    }
  }

  bool IsMouseButtonDown(int button) const
  {
    return mouseButtonsDown.find(button) != mouseButtonsDown.end();
  }

  void SetMousePosition(const glm::vec2& position)
  {
    mousePosition = position;
  }

  const glm::vec2& GetMousePosition() const
  {
    return mousePosition;
  }

  void AddScrollDelta(float delta)
  {
    scrollDelta += delta;
  }

  float GetScrollDelta() const
  {
    return scrollDelta;
  }

  void ResetFrameTransientState()
  {
    scrollDelta = 0.0f;
  }

private:
  std::unordered_set<int> keysDown;
  std::unordered_set<int> mouseButtonsDown;
  glm::vec2 mousePosition{0.0f, 0.0f};
  float scrollDelta = 0.0f;
};