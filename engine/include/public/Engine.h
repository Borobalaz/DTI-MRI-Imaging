#pragma once

#include <functional>
#include <memory>

#include <glm/glm.hpp>

#include "graphics/IGraphicsContextProvider.h"
#include "graphics/ScreenFrame.h"
#include "inspection/InspectField.h"
#include "inspection/IInspectionService.h"
#include "log/ILogService.h"
#include "settings/ISettingsService.h"

class EngineRuntime;

/**
 * @brief Public façade for the engine library. This is the only class UI code
 *  should depend on; all other engine headers are private implementation details.
 *
 *  The engine can be driven in one of two modes:
 *
 *  - Threaded mode (Start/Stop): the engine owns its render thread and the graphics context
 *    (obtained from a host IGraphicsContextProvider). Each tick it drains input, updates the
 *    scene, renders every screen offscreen and publishes the frames as CPU pixels
 *    (GetScreenFrame). The host never makes graphics-API calls.
 *
 *  - Sync mode (InitializeOpenGL/CreateScene/Update/Render/ShutdownGraphics): the host drives
 *    the loop itself on a thread that has the context current. Both modes run the same tick
 *    body; in sync mode the caller's thread simply is the GL thread.
 *
 *  Calling a sync-mode method while IsRunning() is a programming error: it asserts in debug
 *  builds and is ignored (with an error log) otherwise.
 *
 *  Callback contract: every callback registered here or on the services (OnFrameReady,
 *  OnProvidersChanged, OnEntriesChanged, OnSettingsChanged) fires on the engine thread in
 *  threaded mode, and on the thread calling Update()/Render() in sync mode. Callbacks never
 *  fire while an engine lock is held, but they must not block on the host UI thread (marshal
 *  to it asynchronously instead), and must not call Stop().
 */
class Engine
{
public:
  // OpenGL function-pointer resolver supplied by the windowing layer
  // (signature-compatible with GLADloadproc).
  using GLProcLoader = void* (*)(const char*);

  using FrameReadyCallback = std::function<void(ScreenId)>;

  struct Ray
  {
    glm::vec3 origin;
    glm::vec3 direction;
  };

  Engine();
  // Calls Stop(), so an engine running in threaded mode joins its thread here.
  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // ---------------------------------------------------------------------------
  // Threaded mode. Start/Stop/IsRunning/SetPaused may be called from any thread
  // other than the engine thread itself.
  // ---------------------------------------------------------------------------

  // Spawns the engine thread and blocks until the context, renderer and scene are ready.
  // Returns false (with no thread left running) if initialization failed, if the engine is
  // already running, or if sync-mode graphics were initialized and not shut down.
  bool Start(std::shared_ptr<IGraphicsContextProvider> provider);

  // Idempotent. Stops and joins the engine thread, which tears down (in order) the scene
  // (cancelling/joining preprocessing), all screens, the renderer and the remaining GPU
  // garbage, then releases and destroys the context. Destroys every screen.
  void Stop();

  bool IsRunning() const;

  // While paused, the engine thread sleeps without ticking (no update, render or callbacks).
  void SetPaused(bool paused);

  // ---------------------------------------------------------------------------
  // Screens: offscreen render targets the engine renders every tick. Thread-safe.
  // ---------------------------------------------------------------------------

  ScreenId CreateScreen();
  void DestroyScreen(ScreenId id);

  // Size in device pixels; a screen with a non-positive size is not rendered.
  void SetScreenSize(ScreenId id, int width, int height);

  // Hands over the newest published frame by swapping it with `out` (no pixel copy; the
  // buffer previously held by `out` is recycled by the engine). Returns false, leaving `out`
  // untouched, when no frame newer than the last one taken exists or the screen is unknown.
  bool GetScreenFrame(ScreenId id, ScreenFrame& out);

  // Invoked once per screen after a new frame for it was published. See the callback contract.
  void OnFrameReady(FrameReadyCallback callback);

  // ---------------------------------------------------------------------------
  // Sync mode: call on the thread that has the graphics context current.
  // ---------------------------------------------------------------------------

  bool InitializeOpenGL(GLProcLoader loadProc);
  void CreateScene();
  void Update(float deltaSeconds);

  // Renders every screen if any exist (publishing their frames like threaded mode does),
  // otherwise renders into the framebuffer currently bound, at SetViewportSize's size.
  void Render();

  // Tears down the scene, all screens and the renderer, and releases the graphics API.
  // The context must still be current.
  void ShutdownGraphics();

  // ---------------------------------------------------------------------------
  // Thread-safe in both modes, from any thread.
  // ---------------------------------------------------------------------------

  // Applied on the next tick. Only used when rendering into the bound framebuffer (sync mode
  // without screens); screens use their own size.
  void SetViewportSize(int width, int height);
  void SetFillColor(const glm::vec3& color);

  // Input is queued and replayed in order at the start of the next tick.
  void OnKeyChanged(int key, bool down);
  // Reflects every OnKeyChanged call made so far, including ones not yet replayed.
  bool IsKeyDown(int key) const;
  void OnMouseButtonChanged(int button, bool down);
  void OnMousePositionChanged(const glm::vec2& position);
  void OnScroll(float delta);

  // The returned services stay valid for the Engine's lifetime; every call on them (and every
  // getter/setter of the fields they return) is serialized with the engine tick. Fields of a
  // provider that has been removed since they were fetched become inert: their getter
  // returns the field's initialValue and their setter does nothing.
  IInspectionService& GetInspectionService();
  ILogService& GetLogService();
  ISettingsService& GetSettingsService();

  // Picking (locks the scene).
  Ray ScreenPointToRay(float ndcX, float ndcY) const;

private:
  std::unique_ptr<EngineRuntime> runtime;
};
