#include "runtime/input/InputEventQueue.h"

#include "scene/input/InputState.h"

void InputEventQueue::Push(const InputEvent& event)
{
  std::lock_guard<std::mutex> lock(mutex);
  pending.push_back(event);

  if (event.kind == InputEventKind::Key)
  {
    if (event.down)
    {
      keysDownMirror.insert(event.code);
    }
    else
    {
      keysDownMirror.erase(event.code);
    }
  }
}

bool InputEventQueue::IsKeyDown(int key) const
{
  std::lock_guard<std::mutex> lock(mutex);
  return keysDownMirror.find(key) != keysDownMirror.end();
}

void InputEventQueue::DrainInto(InputState& state)
{
  std::vector<InputEvent> events;
  {
    std::lock_guard<std::mutex> lock(mutex);
    events.swap(pending);
  }

  for (const InputEvent& event : events)
  {
    switch (event.kind)
    {
      case InputEventKind::Key:
        state.SetKeyDown(event.code, event.down);
        break;
      case InputEventKind::MouseButton:
        state.SetMouseButtonDown(event.code, event.down);
        break;
      case InputEventKind::MousePosition:
        state.SetMousePosition(event.position);
        break;
      case InputEventKind::Scroll:
        state.AddScrollDelta(event.scrollDelta);
        break;
    }
  }
}
