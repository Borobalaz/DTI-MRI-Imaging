#include "runtime/EngineRuntime.h"

#include <cassert>
#include <cmath>
#include <exception>
#include <string>
#include <utility>

#include <glm/gtc/matrix_inverse.hpp>

#include "DtiVolumeScene.h"
#include "runtime/dashboard/SceneDashboardService.h"
#include "runtime/inspection/SceneInspectionService.h"
#include "foundation/log/EngineLog.h"
#include "render/renderer/ForwardRenderer.h"
#include "scene/extraction/ExtractionRegistry.h"
#include "scene/extraction/RenderFrameBuilder.h"
#include "scene/Scene.h"
#include "runtime/settings/EngineSettings.h"
#include "runtime/settings/EngineSettingsCatalog.h"

namespace
{
  using SceneLock = std::lock_guard<std::recursive_mutex>;

  constexpr float kFirstFrameDeltaSeconds = 1.0f / 60.0f;
  // Pacing sleeps until this much before the frame deadline, then yield-spins the rest:
  // OS sleeps routinely overshoot by about a millisecond.
  constexpr std::chrono::milliseconds kPacingSpinWindow{2};
  // A tick whose locked update + render exceeds this (roughly two 60 Hz frames) is logged.
  constexpr double kSlowTickThresholdMilliseconds = 33.0;

  // The context provider of the engine thread running on this thread. The graphics-API loader takes a plain
  // function pointer, so it reaches the provider through this trampoline.
  thread_local IGraphicsContextProvider* currentContextProvider = nullptr;

  void* ResolveProcAddressThroughProvider(const char* name)
  {
    return currentContextProvider ? currentContextProvider->GetProcAddress(name) : nullptr;
  }

  double MillisecondsSince(std::chrono::steady_clock::time_point start)
  {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  }

  std::string FormatMilliseconds(double milliseconds)
  {
    return std::to_string(static_cast<long long>(std::llround(milliseconds)));
  }
}

EngineRuntime::EngineRuntime()
  : extractionRegistry(std::make_unique<ExtractionRegistry>()),
    renderFrameBuilder(std::make_unique<RenderFrameBuilder>()),
    lockedInspectionService(sceneMutex),
    lockedSettingsService(sceneMutex, settingsService),
    lockedSceneDashboardService(sceneMutex)
{
  RegisterEngineSettingsCatalog();
}

EngineRuntime::~EngineRuntime()
{
  Stop();
}

// ---------------------------------------------------------------------------
// Threaded mode
// ---------------------------------------------------------------------------

bool EngineRuntime::Start(std::shared_ptr<IGraphicsContextProvider> provider)
{
  if (!provider)
  {
    EngineLog::Error("Engine::Start called without a graphics context provider.", "Engine");
    return false;
  }

  if (running.load() || engineThread.joinable())
  {
    EngineLog::Error("Engine::Start called while the engine is already running.", "Engine");
    return false;
  }

  {
    SceneLock lock(sceneMutex);
    if (graphicsApiInitialized)
    {
      EngineLog::Error("Engine::Start called while sync-mode graphics are initialized; call ShutdownGraphics first.",
                       "Engine");
      return false;
    }
  }

  stopRequested.store(false);
  running.store(true);

  std::promise<bool> ready;
  std::future<bool> readyResult = ready.get_future();
  engineThread = std::thread(&EngineRuntime::ThreadMain, this, std::move(provider), std::move(ready));

  if (readyResult.get())
  {
    return true;
  }

  engineThread.join();
  running.store(false);
  return false;
}

void EngineRuntime::Stop()
{
  if (!engineThread.joinable())
  {
    return;
  }

  if (std::this_thread::get_id() == engineThread.get_id())
  {
    assert(!"Engine::Stop must not be called from the engine thread (e.g. from a callback)");
    EngineLog::Error("Engine::Stop called from the engine thread; ignored.", "Engine");
    return;
  }

  {
    std::lock_guard<std::mutex> lock(loopMutex);
    stopRequested.store(true);
  }
  loopWake.notify_all();

  engineThread.join();
  running.store(false);
}

bool EngineRuntime::IsRunning() const
{
  return running.load();
}

void EngineRuntime::SetPaused(bool pausedValue)
{
  {
    std::lock_guard<std::mutex> lock(loopMutex);
    paused.store(pausedValue);
  }
  loopWake.notify_all();
}

void EngineRuntime::ThreadMain(std::shared_ptr<IGraphicsContextProvider> provider, std::promise<bool> ready)
{
  currentContextProvider = provider.get();

  bool contextCreated = false;
  bool contextCurrent = false;
  bool initialized = false;
  try
  {
    initialized = InitializeOnEngineThread(*provider, contextCreated, contextCurrent);
  }
  catch (const std::exception& exception)
  {
    EngineLog::Error(std::string("Engine thread initialization failed: ") + exception.what(), "Engine");
  }

  if (initialized)
  {
    ready.set_value(true);
    RunLoop();
  }

  if (contextCurrent)
  {
    TeardownGraphics();
    provider->DoneCurrent();
  }
  if (contextCreated)
  {
    provider->DestroyContext();
  }
  currentContextProvider = nullptr;

  if (!initialized)
  {
    ready.set_value(false);
  }
}

bool EngineRuntime::InitializeOnEngineThread(IGraphicsContextProvider& provider, bool& contextCreated, bool& contextCurrent)
{
  contextCreated = provider.CreateContext();
  if (!contextCreated)
  {
    EngineLog::Error("Graphics context provider failed to create a context.", "Engine");
    return false;
  }

  contextCurrent = provider.MakeCurrent();
  if (!contextCurrent)
  {
    EngineLog::Error("Graphics context provider failed to make its context current.", "Engine");
    return false;
  }

  SceneLock lock(sceneMutex);
  if (!InitializeGraphicsLocked(&ResolveProcAddressThroughProvider))
  {
    EngineLog::Error("Failed to load graphics API entry points on the engine thread.", "Engine");
    return false;
  }

  CreateSceneLocked();
  return true;
}

void EngineRuntime::RunLoop()
{
  Clock::time_point lastTickStart{};
  bool hasLastTick = false;

  while (!stopRequested.load())
  {
    if (paused.load())
    {
      std::unique_lock<std::mutex> lock(loopMutex);
      loopWake.wait(lock, [this]() { return stopRequested.load() || !paused.load(); });
      // Don't feed the paused interval into the next update as one huge time step.
      hasLastTick = false;
      continue;
    }

    const Clock::time_point tickStart = Clock::now();
    const float deltaSeconds = hasLastTick
        ? std::chrono::duration<float>(tickStart - lastTickStart).count()
        : kFirstFrameDeltaSeconds;
    lastTickStart = tickStart;
    hasLastTick = true;

    RunTick(deltaSeconds, tickStart);
    PaceTick(tickStart);
  }
}

void EngineRuntime::RunTick(float deltaSeconds, Clock::time_point tickStart)
{
  ChangeFlags changes;
  std::vector<ScreenWork> renderedScreens;
  double updateMilliseconds = 0.0;
  double renderMilliseconds = 0.0;
  {
    SceneLock lock(sceneMutex);
    const Clock::time_point updateStart = Clock::now();
    changes = UpdateLocked(deltaSeconds);
    updateMilliseconds = MillisecondsSince(updateStart);

    const Clock::time_point renderStart = Clock::now();
    renderedScreens = RenderLocked(false);
    renderMilliseconds = MillisecondsSince(renderStart);
  }

  // Surfaces hitches (e.g. a dataset apply or a large lazy texture upload) without per-frame noise.
  if (updateMilliseconds + renderMilliseconds > kSlowTickThresholdMilliseconds)
  {
    EngineLog::Message("Slow tick: update " + FormatMilliseconds(updateMilliseconds) + " ms, render " +
                           FormatMilliseconds(renderMilliseconds) + " ms",
                       "EngineRuntime");
  }

  const double fps = deltaSeconds > 1e-6f ? 1.0 / static_cast<double>(deltaSeconds) : 0.0;
  PublishScreens(renderedScreens, tickStart, fps);
  NotifyChanges(changes);
}

void EngineRuntime::PaceTick(Clock::time_point tickStart)
{
  const double maxFramerate = EngineSettings::GetNumber("engine.maxFramerate", 60.0);
  if (!(maxFramerate > 0.0))
  {
    return; // uncapped
  }

  const Clock::time_point deadline = tickStart
      + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / maxFramerate));

  // A condition-variable wait rather than a plain sleep, so Stop() interrupts pacing at once.
  const Clock::time_point sleepUntil = deadline - kPacingSpinWindow;
  if (Clock::now() < sleepUntil)
  {
    std::unique_lock<std::mutex> lock(loopMutex);
    loopWake.wait_until(lock, sleepUntil, [this]() { return stopRequested.load(); });
  }

  while (Clock::now() < deadline && !stopRequested.load())
  {
    std::this_thread::yield();
  }
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

ScreenId EngineRuntime::CreateScreen()
{
  std::lock_guard<std::mutex> lock(screensMutex);
  const ScreenId id = nextScreenId++;
  screens.emplace(id, ScreenEntry{std::make_shared<ScreenSlot>()});
  return id;
}

void EngineRuntime::DestroyScreen(ScreenId id)
{
  // The slot may outlive this call (a tick in flight holds it); its RenderTarget queues its GPU
  // objects for deletion on the GL thread whichever thread releases it last.
  std::lock_guard<std::mutex> lock(screensMutex);
  screens.erase(id);
}

void EngineRuntime::SetScreenSize(ScreenId id, int width, int height)
{
  std::lock_guard<std::mutex> lock(screensMutex);
  const auto screen = screens.find(id);
  if (screen != screens.end())
  {
    screen->second.width = width;
    screen->second.height = height;
  }
}

bool EngineRuntime::GetScreenFrame(ScreenId id, ScreenFrame& out)
{
  std::shared_ptr<ScreenSlot> slot;
  {
    std::lock_guard<std::mutex> lock(screensMutex);
    const auto screen = screens.find(id);
    if (screen == screens.end())
    {
      return false;
    }
    slot = screen->second.slot;
  }

  std::lock_guard<std::mutex> lock(slot->frameMutex);
  if (!slot->hasNewFrame)
  {
    return false;
  }

  std::swap(out, slot->ready);
  slot->hasNewFrame = false;
  return true;
}

void EngineRuntime::OnFrameReady(Engine::FrameReadyCallback callback)
{
  std::lock_guard<std::mutex> lock(frameReadyCallbackMutex);
  frameReadyCallback = std::move(callback);
}

std::vector<EngineRuntime::ScreenWork> EngineRuntime::CollectScreenWork()
{
  std::vector<ScreenWork> work;
  std::lock_guard<std::mutex> lock(screensMutex);
  work.reserve(screens.size());
  for (const auto& [id, entry] : screens)
  {
    work.push_back(ScreenWork{id, entry.slot, entry.width, entry.height});
  }
  return work;
}

// ---------------------------------------------------------------------------
// Sync mode
// ---------------------------------------------------------------------------

bool EngineRuntime::IsSyncCallAllowed(const char* methodName) const
{
  if (!running.load())
  {
    return true;
  }

  assert(!"Sync-mode Engine method called while the engine thread is running");
  EngineLog::Error(std::string("Engine::") + methodName + " is a sync-mode call and the engine thread is running; ignored.",
                   "Engine");
  return false;
}

bool EngineRuntime::InitializeOpenGL(Engine::GLProcLoader loadProc)
{
  if (!IsSyncCallAllowed("InitializeOpenGL"))
  {
    return false;
  }

  SceneLock lock(sceneMutex);
  return InitializeGraphicsLocked(loadProc);
}

void EngineRuntime::CreateScene()
{
  if (!IsSyncCallAllowed("CreateScene"))
  {
    return;
  }

  SceneLock lock(sceneMutex);
  CreateSceneLocked();
}

void EngineRuntime::Update(float deltaSeconds)
{
  if (!IsSyncCallAllowed("Update"))
  {
    return;
  }

  ChangeFlags changes;
  {
    SceneLock lock(sceneMutex);
    changes = UpdateLocked(deltaSeconds);
  }
  NotifyChanges(changes);
}

void EngineRuntime::Render()
{
  if (!IsSyncCallAllowed("Render"))
  {
    return;
  }

  const Clock::time_point renderStart = Clock::now();
  std::vector<ScreenWork> renderedScreens;
  {
    SceneLock lock(sceneMutex);
    renderedScreens = RenderLocked(true);
  }

  const double secondsSinceLastRender = lastSyncRenderStart == Clock::time_point{}
      ? 0.0
      : std::chrono::duration<double>(renderStart - lastSyncRenderStart).count();
  lastSyncRenderStart = renderStart;

  const double fps = secondsSinceLastRender > 1e-6 ? 1.0 / secondsSinceLastRender : 0.0;
  PublishScreens(renderedScreens, renderStart, fps);
}

void EngineRuntime::ShutdownGraphics()
{
  if (!IsSyncCallAllowed("ShutdownGraphics"))
  {
    return;
  }

  TeardownGraphics();
}

// ---------------------------------------------------------------------------
// Tick body
// ---------------------------------------------------------------------------

bool EngineRuntime::InitializeGraphicsLocked(GraphicsProcLoader loadProc)
{
  if (!InitializeGraphicsApi(loadProc))
  {
    return false;
  }
  graphicsApiInitialized = true;

  // The renderer owns GPU resources, so it is created here (context current), not in the ctor.
  renderer = std::make_unique<ForwardRenderer>();
  renderer->InitializeGLState();
  return true;
}

void EngineRuntime::CreateSceneLocked()
{
  lockedInspectionService.Detach();
  sceneInspectionService.reset();
  lockedSceneDashboardService.Detach();
  sceneDashboardService.reset();

  scene = std::make_unique<DtiVolumeScene>();
  scene->RebuildInspectProviders();
  sceneInspectionService = std::make_unique<SceneInspectionService>(*scene);
  lockedInspectionService.Attach(*sceneInspectionService, *scene);
  sceneDashboardService = std::make_unique<SceneDashboardService>(*scene);
  lockedSceneDashboardService.Attach(*sceneDashboardService, *scene);
}

EngineRuntime::ChangeFlags EngineRuntime::UpdateLocked(float deltaSeconds)
{
  ChangeFlags changes;
  changes.logChanged = logService.PollForEntryChanges();
  changes.settingsChanged = settingsService.PollForSettingsChanges();

  inputQueue.DrainInto(inputState);

  if (!scene)
  {
    return changes;
  }

  scene->ReloadShadersIfChanged();
  scene->SetInputState(inputState);
  scene->Update(deltaSeconds);

  if (sceneInspectionService)
  {
    changes.providersChanged = sceneInspectionService->PollForProviderChanges();
  }
  if (sceneDashboardService)
  {
    changes.sceneDashboardChanged = sceneDashboardService->PollForFieldChanges();
  }
  return changes;
}

std::vector<EngineRuntime::ScreenWork> EngineRuntime::RenderLocked(bool allowBoundFramebuffer)
{
  std::vector<ScreenWork> renderedScreens;
  if (!scene || !renderer)
  {
    return renderedScreens;
  }

  CollectGraphicsGarbage();

  glm::vec3 currentFillColor;
  int currentViewportWidth = 0;
  int currentViewportHeight = 0;
  {
    std::lock_guard<std::mutex> lock(hostStateMutex);
    currentFillColor = fillColor;
    currentViewportWidth = viewportWidth;
    currentViewportHeight = viewportHeight;
  }

  renderer->SetFillColor(currentFillColor);
  renderer->SetPostProcessingEnabled(EngineSettings::GetBool("postprocessing.enabled", true));
  ++frameCounter;

  const std::vector<ScreenWork> screenWork = CollectScreenWork();
  if (screenWork.empty())
  {
    if (allowBoundFramebuffer)
    {
      if (currentViewportHeight > 0)
      {
        scene->SetCameraAspect(static_cast<float>(currentViewportWidth) / static_cast<float>(currentViewportHeight));
      }
      renderer->Resize(currentViewportWidth, currentViewportHeight);
      DrawSceneLocked(nullptr);
    }
  }
  else
  {
    for (const ScreenWork& work : screenWork)
    {
      if (work.width <= 0 || work.height <= 0)
      {
        continue;
      }

      work.slot->target.Resize(work.width, work.height);
      if (!work.slot->target.IsValid())
      {
        continue;
      }

      scene->SetCameraAspect(static_cast<float>(work.width) / static_cast<float>(work.height));
      DrawSceneLocked(&work.slot->target);
      renderedScreens.push_back(work);
    }
  }

  inputState.ResetFrameTransientState();
  return renderedScreens;
}

void EngineRuntime::DrawSceneLocked(const RenderTarget* target)
{
  const SceneSnapshot snapshot = scene->CreateSnapshot();
  const RenderFrame frame = renderFrameBuilder->Build(snapshot, renderer->GetDescriptor(), *extractionRegistry);
  if (target)
  {
    renderer->Draw(frame, *target);
  }
  else
  {
    renderer->Draw(frame);
  }
}

void EngineRuntime::PublishScreens(const std::vector<ScreenWork>& renderedScreens, Clock::time_point tickStart, double fps)
{
  if (renderedScreens.empty())
  {
    return;
  }

  std::vector<ScreenId> publishedIds;
  publishedIds.reserve(renderedScreens.size());

  for (const ScreenWork& work : renderedScreens)
  {
    ScreenSlot& slot = *work.slot;
    if (!slot.target.ReadPixelsRGBA8(slot.back.pixels))
    {
      continue;
    }

    slot.back.width = slot.target.GetWidth();
    slot.back.height = slot.target.GetHeight();
    slot.back.frameIndex = frameCounter;
    slot.back.frameTimeMs = MillisecondsSince(tickStart);
    slot.back.fps = fps;

    {
      std::lock_guard<std::mutex> lock(slot.frameMutex);
      std::swap(slot.back, slot.ready);
      slot.hasNewFrame = true;
    }
    publishedIds.push_back(work.id);
  }

  Engine::FrameReadyCallback callback;
  {
    std::lock_guard<std::mutex> lock(frameReadyCallbackMutex);
    callback = frameReadyCallback;
  }

  if (callback)
  {
    for (const ScreenId id : publishedIds)
    {
      callback(id);
    }
  }
}

// Reads sceneInspectionService/sceneDashboardService without the scene lock: it is only replaced on the GL thread,
// which is also the only thread that calls this.
void EngineRuntime::NotifyChanges(const ChangeFlags& changes)
{
  if (changes.logChanged)
  {
    logService.NotifyChanged();
  }
  if (changes.settingsChanged)
  {
    settingsService.NotifyChanged();
  }
  if (changes.providersChanged && sceneInspectionService)
  {
    sceneInspectionService->NotifyChanged();
  }
  if (changes.sceneDashboardChanged && sceneDashboardService)
  {
    sceneDashboardService->NotifyChanged();
  }
}

void EngineRuntime::TeardownGraphics()
{
  // Detached under the lock so the host can no longer reach the scene; destroyed outside it,
  // because the scene's destructor may wait for preprocessing work to stop.
  std::unique_ptr<SceneInspectionService> retiredInspectionService;
  std::unique_ptr<SceneDashboardService> retiredDashboardService;
  std::unique_ptr<Scene> retiredScene;
  {
    SceneLock lock(sceneMutex);
    lockedInspectionService.Detach();
    retiredInspectionService = std::move(sceneInspectionService);
    lockedSceneDashboardService.Detach();
    retiredDashboardService = std::move(sceneDashboardService);
    retiredScene = std::move(scene);
  }
  retiredInspectionService.reset();
  retiredDashboardService.reset();
  retiredScene.reset();

  {
    std::lock_guard<std::mutex> lock(screensMutex);
    screens.clear();
  }

  SceneLock lock(sceneMutex);
  renderer.reset();

  if (graphicsApiInitialized)
  {
    CollectGraphicsGarbage();
    ShutdownGraphicsApi();
    graphicsApiInitialized = false;
  }
}

// ---------------------------------------------------------------------------
// Thread-safe host state, input, services, picking
// ---------------------------------------------------------------------------

void EngineRuntime::SetViewportSize(int width, int height)
{
  std::lock_guard<std::mutex> lock(hostStateMutex);
  viewportWidth = width;
  viewportHeight = height;
}

void EngineRuntime::SetFillColor(const glm::vec3& color)
{
  std::lock_guard<std::mutex> lock(hostStateMutex);
  fillColor = color;
}

void EngineRuntime::PushInput(const InputEvent& event)
{
  inputQueue.Push(event);
}

bool EngineRuntime::IsKeyDown(int key) const
{
  return inputQueue.IsKeyDown(key);
}

IInspectionService& EngineRuntime::GetInspectionService()
{
  return lockedInspectionService;
}

ILogService& EngineRuntime::GetLogService()
{
  return logService;
}

ISettingsService& EngineRuntime::GetSettingsService()
{
  return lockedSettingsService;
}

ISceneDashboardService& EngineRuntime::GetSceneDashboardService()
{
  return lockedSceneDashboardService;
}

Engine::Ray EngineRuntime::ScreenPointToRay(float ndcX, float ndcY) const
{
  SceneLock lock(sceneMutex);
  if (!scene)
  {
    return Engine::Ray{glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
  }

  std::shared_ptr<Camera> camera = scene->GetCamera();
  if (!camera)
  {
    return Engine::Ray{glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
  }

  const glm::mat4 viewMatrix = camera->GetViewMatrix();
  const glm::mat4 projectionMatrix = camera->GetProjectionMatrix();
  const glm::mat4 inverseViewProjection = glm::inverse(projectionMatrix * viewMatrix);

  const glm::vec4 nearClip(ndcX, ndcY, -1.0f, 1.0f);
  const glm::vec4 farClip(ndcX, ndcY, 1.0f, 1.0f);

  glm::vec4 nearWorld = inverseViewProjection * nearClip;
  glm::vec4 farWorld = inverseViewProjection * farClip;
  if (std::abs(nearWorld.w) > 1e-6f)
  {
    nearWorld /= nearWorld.w;
  }
  if (std::abs(farWorld.w) > 1e-6f)
  {
    farWorld /= farWorld.w;
  }

  Engine::Ray ray;
  ray.origin = camera->GetPosition();
  ray.direction = glm::normalize(glm::vec3(farWorld - nearWorld));
  return ray;
}
