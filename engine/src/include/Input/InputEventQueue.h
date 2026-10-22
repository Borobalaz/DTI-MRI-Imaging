#pragma once

#include <mutex>
#include <unordered_set>
#include <vector>

#include <glm/vec2.hpp>

class InputState;

enum class InputEventKind
{
  Key,
  MouseButton,
  MousePosition,
  Scroll
};

struct InputEvent
{
  InputEventKind kind = InputEventKind::Key;
  int code = 0;          // key or mouse button (Key / MouseButton)
  bool down = false;     // Key / MouseButton
  glm::vec2 position{0.0f, 0.0f}; // MousePosition
  float scrollDelta = 0.0f;       // Scroll
};

/**
 * @brief Hands input from the host thread(s) (Push) to the engine tick (DrainInto) without
 *  either side touching the engine's InputState concurrently. Events are replayed in the order
 *  they were pushed, so presses/releases within one tick are not lost.
 */
class InputEventQueue
{
public:
  void Push(const InputEvent& event);

  // Producer-side view of the keys currently held, including events not drained yet, so a
  // host can query a modifier right after reporting it.
  bool IsKeyDown(int key) const;

  // Replays every pending event into `state`, in push order, and empties the queue.
  void DrainInto(InputState& state);

private:
  mutable std::mutex mutex;
  std::vector<InputEvent> pending;
  std::unordered_set<int> keysDownMirror;
};
