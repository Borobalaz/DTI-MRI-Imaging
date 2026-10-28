#include "Engine.h"

#include "runtime/input/InputEventQueue.h"
#include "runtime/EngineRuntime.h"

// Engine is a thin façade: all state, threading and locking live in EngineRuntime.

Engine::Engine()
  : runtime(std::make_unique<EngineRuntime>())
{
}

// EngineRuntime's destructor stops the engine thread first.
Engine::~Engine() = default;

bool Engine::Start(std::shared_ptr<IGraphicsContextProvider> provider)
{
  return runtime->Start(std::move(provider));
}

void Engine::Stop()
{
  runtime->Stop();
}

bool Engine::IsRunning() const
{
  return runtime->IsRunning();
}

void Engine::SetPaused(bool paused)
{
  runtime->SetPaused(paused);
}

ScreenId Engine::CreateScreen()
{
  return runtime->CreateScreen();
}

void Engine::DestroyScreen(ScreenId id)
{
  runtime->DestroyScreen(id);
}

void Engine::SetScreenSize(ScreenId id, int width, int height)
{
  runtime->SetScreenSize(id, width, height);
}

bool Engine::GetScreenFrame(ScreenId id, ScreenFrame& out)
{
  return runtime->GetScreenFrame(id, out);
}

void Engine::OnFrameReady(FrameReadyCallback callback)
{
  runtime->OnFrameReady(std::move(callback));
}

bool Engine::InitializeOpenGL(GLProcLoader loadProc)
{
  return runtime->InitializeOpenGL(loadProc);
}

void Engine::CreateScene()
{
  runtime->CreateScene();
}

void Engine::Update(float deltaSeconds)
{
  runtime->Update(deltaSeconds);
}

void Engine::Render()
{
  runtime->Render();
}

void Engine::ShutdownGraphics()
{
  runtime->ShutdownGraphics();
}

void Engine::SetViewportSize(int width, int height)
{
  runtime->SetViewportSize(width, height);
}

void Engine::SetFillColor(const glm::vec3& color)
{
  runtime->SetFillColor(color);
}

void Engine::OnKeyChanged(int key, bool down)
{
  InputEvent event;
  event.kind = InputEventKind::Key;
  event.code = key;
  event.down = down;
  runtime->PushInput(event);
}

bool Engine::IsKeyDown(int key) const
{
  return runtime->IsKeyDown(key);
}

void Engine::OnMouseButtonChanged(int button, bool down)
{
  InputEvent event;
  event.kind = InputEventKind::MouseButton;
  event.code = button;
  event.down = down;
  runtime->PushInput(event);
}

void Engine::OnMousePositionChanged(const glm::vec2& position)
{
  InputEvent event;
  event.kind = InputEventKind::MousePosition;
  event.position = position;
  runtime->PushInput(event);
}

void Engine::OnScroll(float delta)
{
  InputEvent event;
  event.kind = InputEventKind::Scroll;
  event.scrollDelta = delta;
  runtime->PushInput(event);
}

IInspectionService& Engine::GetInspectionService()
{
  return runtime->GetInspectionService();
}

ILogService& Engine::GetLogService()
{
  return runtime->GetLogService();
}

ISettingsService& Engine::GetSettingsService()
{
  return runtime->GetSettingsService();
}

Engine::Ray Engine::ScreenPointToRay(float ndcX, float ndcY) const
{
  return runtime->ScreenPointToRay(ndcX, ndcY);
}
