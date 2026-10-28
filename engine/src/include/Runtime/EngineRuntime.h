#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <glm/glm.hpp>

#include "Engine.h"
#include "Input/InputEventQueue.h"
#include "scene/input/InputState.h"
#include "Inspection/LockedInspectionService.h"
#include "Log/EngineLogService.h"
#include "render/GraphicsApi.h"
#include "render/RenderTarget.h"
#include "Settings/LockedSettingsService.h"
#include "Settings/SettingsService.h"

class Scene;
class ForwardRenderer;
class ExtractionRegistry;
class RenderFrameBuilder;
class SceneInspectionService;

/**
 * @brief Implementation behind the Engine façade (pimpl, so the public header stays free of
 *  <thread>/<mutex>). Owns the scene, renderer and services, and in threaded mode the engine
 *  thread that ticks them.
 *
 *  Locking:
 *  - sceneMutex (recursive) guards the scene, renderer, render-frame builder, InputState and
 *    the scene inspection service. A tick holds it for input drain + Update + snapshot/Draw
 *    (Draw must stay inside: render commands reference live UniformProviders). Readback,
 *    frame publishing, callbacks and pacing run outside it.
 *  - screensMutex guards the screen map and requested sizes; each ScreenSlot's frameMutex
 *    guards only its published (ready) frame.
 *  - Lock order: sceneMutex -> screensMutex -> frameMutex; callbacks run with no lock held.
 *
 *  Only the GL thread (engine thread in threaded mode, the sync-mode caller otherwise) touches
 *  the graphics API, the renderer, and each slot's RenderTarget and back buffer.
 */
class EngineRuntime
{
public:
  EngineRuntime();
  ~EngineRuntime();

  EngineRuntime(const EngineRuntime&) = delete;
  EngineRuntime& operator=(const EngineRuntime&) = delete;

  // Threaded mode
  bool Start(std::shared_ptr<IGraphicsContextProvider> provider);
  void Stop();
  bool IsRunning() const;
  void SetPaused(bool paused);

  // Screens
  ScreenId CreateScreen();
  void DestroyScreen(ScreenId id);
  void SetScreenSize(ScreenId id, int width, int height);
  bool GetScreenFrame(ScreenId id, ScreenFrame& out);
  void OnFrameReady(Engine::FrameReadyCallback callback);

  // Sync mode
  bool InitializeOpenGL(Engine::GLProcLoader loadProc);
  void CreateScene();
  void Update(float deltaSeconds);
  void Render();
  void ShutdownGraphics();

  // Thread-safe host state and input
  void SetViewportSize(int width, int height);
  void SetFillColor(const glm::vec3& color);
  void PushInput(const InputEvent& event);
  bool IsKeyDown(int key) const;

  IInspectionService& GetInspectionService();
  ILogService& GetLogService();
  ISettingsService& GetSettingsService();

  Engine::Ray ScreenPointToRay(float ndcX, float ndcY) const;

private:
  using Clock = std::chrono::steady_clock;

  struct ScreenSlot
  {
    RenderTarget target; // GL thread only
    ScreenFrame back;    // GL thread only: readback destination, swapped into `ready`

    std::mutex frameMutex;
    ScreenFrame ready;        // guarded by frameMutex
    bool hasNewFrame = false; // guarded by frameMutex
  };

  struct ScreenEntry
  {
    std::shared_ptr<ScreenSlot> slot;
    int width = 0;
    int height = 0;
  };

  // A screen as seen by one tick: the slot is kept alive for the whole tick even if the host
  // destroys the screen meanwhile.
  struct ScreenWork
  {
    ScreenId id = kInvalidScreenId;
    std::shared_ptr<ScreenSlot> slot;
    int width = 0;
    int height = 0;
  };

  struct ChangeFlags
  {
    bool logChanged = false;
    bool settingsChanged = false;
    bool providersChanged = false;
  };

  // Logs (and asserts) when a sync-mode method is called while the engine thread runs.
  bool IsSyncCallAllowed(const char* methodName) const;

  // --- Tick body shared by sync and threaded mode ---------------------------
  // Caller holds sceneMutex.
  bool InitializeGraphicsLocked(GraphicsProcLoader loadProc);
  void CreateSceneLocked();
  ChangeFlags UpdateLocked(float deltaSeconds);
  // Renders every screen; with no screens, renders into the bound framebuffer if allowed.
  // Returns the screens that were rendered (to be read back by PublishScreens).
  std::vector<ScreenWork> RenderLocked(bool allowBoundFramebuffer);
  void DrawSceneLocked(const RenderTarget* target);
  // No engine lock held.
  void PublishScreens(const std::vector<ScreenWork>& renderedScreens, Clock::time_point tickStart, double fps);
  void NotifyChanges(const ChangeFlags& changes);

  std::vector<ScreenWork> CollectScreenWork();
  // GL thread, no lock held: scene -> screens -> renderer -> GPU garbage -> graphics API.
  void TeardownGraphics();

  // --- Engine thread ----------------------------------------------------------
  void ThreadMain(std::shared_ptr<IGraphicsContextProvider> provider, std::promise<bool> ready);
  bool InitializeOnEngineThread(IGraphicsContextProvider& provider, bool& contextCreated, bool& contextCurrent);
  void RunLoop();
  void RunTick(float deltaSeconds, Clock::time_point tickStart);
  void PaceTick(Clock::time_point tickStart);

  // Threading (threaded mode)
  std::thread engineThread;
  std::atomic<bool> running{false};
  std::atomic<bool> stopRequested{false};
  std::atomic<bool> paused{false};
  std::mutex loopMutex;
  std::condition_variable loopWake;

  // Scene state, guarded by sceneMutex
  mutable std::recursive_mutex sceneMutex;
  std::unique_ptr<Scene> scene;
  std::unique_ptr<SceneInspectionService> sceneInspectionService; // declared after scene: destroyed first
  std::unique_ptr<ForwardRenderer> renderer;
  std::unique_ptr<ExtractionRegistry> extractionRegistry;
  std::unique_ptr<RenderFrameBuilder> renderFrameBuilder;
  InputState inputState;
  bool graphicsApiInitialized = false;

  // Services (internally synchronized) and their locking decorators
  EngineLogService logService;
  SettingsService settingsService;
  LockedInspectionService lockedInspectionService;
  LockedSettingsService lockedSettingsService;

  InputEventQueue inputQueue;

  // Host state applied at the next render
  std::mutex hostStateMutex;
  glm::vec3 fillColor{0.0f};
  int viewportWidth = 0;
  int viewportHeight = 0;

  // Screens
  std::mutex screensMutex;
  std::map<ScreenId, ScreenEntry> screens;
  ScreenId nextScreenId = 1;

  std::mutex frameReadyCallbackMutex;
  Engine::FrameReadyCallback frameReadyCallback;

  // GL thread only: frame metrics
  std::uint64_t frameCounter = 0;
  Clock::time_point lastSyncRenderStart{};
};
