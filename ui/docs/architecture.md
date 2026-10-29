# UI Architecture

## Purpose

The UI is the `app_qt` CMake executable. It owns these parts:

- Qt application startup and the main window;
- the viewport that displays engine frames;
- inspector presentation, object-list controls, and styles;
- Qt-to-engine adaptation;
- the Qt-side graphics context provider the engine renders with.

Its implementation is under `ui/src`, and its headers are under `ui/include`. The UI makes **no graphics-API calls**. The engine renders on its own thread and hands back CPU pixels.

Primary build evidence: `CMakeLists.txt` (`add_executable(app_qt ...)`). The application entry point is `ui/src/app/main.cpp`.

## Top-Down Decomposition

| Module | Role | Main inputs | Main outputs | Ownership |
|---|---|---|---|---|
| Application entry | Configures Qt and the default OpenGL surface format (read by the context provider) | Process arguments and Qt runtime | `QApplication`, `WidgetsMainWindow`, event loop | Qt owns the application loop; `main` owns the top-level window |
| Main window | Composes the main layout, starts/stops the engine, coordinates UI signals | Viewport, scene dashboard, inspector, object list, stats, log widgets | Window layout (viewport, object list, stats, log, scene dashboard and inspector are all movable/resizable/floatable `QDockWidget`s; no central widget) and cross-widget synchronization | Window owns its child widgets through Qt parent ownership/raw child pointers |
| Viewport | Plain `QWidget` that owns the `Engine`, displays the frames the engine publishes, and forwards input/size | Qt input/resize events, `ScreenFrame`s | Engine input calls, screen size, render statistics | Viewport owns the `Engine`, the context provider, the inspector/settings/scene-dashboard adapters and `RenderStatistics` |
| Graphics context provider | Supplies the engine thread with a Qt-created OpenGL context | Default `QSurfaceFormat` | `IGraphicsContextProvider` implementation | Owned (as `shared_ptr`) by the viewport; shared with the engine while it runs |
| Scene inspector adapter | Converts engine inspection providers and fields into Qt objects/signals | `IInspectionService`, ray-selection requests | `QObjectList`, QVariant metadata/values, Qt signals | Adapter owns Qt field wrappers and snapshots; engine owns provider state |
| Scene dashboard adapter | Converts the active scene's scene-wide fields (preprocessing, tractography, fODF, glyphs, rotation) into Qt objects/signals | `ISceneDashboardService` | `QObjectList`, QVariant values, Qt signals | Adapter owns Qt field wrappers and snapshots; engine owns the scene |
| Inspector widgets | Presents editable fields and sends user edits back through callbacks | Qt field objects and QVariant values | Qt controls and field mutation callbacks | Qt parent/widget ownership controls visual lifetime |
| Object list | Displays selectable engine inspection providers and visibility controls | Provider list and selection/visibility signals | Selection and visibility requests | Main window owns the list widget |
| Styles, statistics and log | Applies theme resources, displays frame metrics and engine log entries | Theme toggle events, `RenderStatistics`, `ILogService` | Qt stylesheets and labels | Window and widgets own presentation state |

## Runtime Flow

1. `ui/src/app/main.cpp` sets a 3.3 core-profile default `QSurfaceFormat` (24-bit depth, 8-bit stencil), creates `QApplication`, and shows `WidgetsMainWindow`. `AA_ShareOpenGLContexts` is not set, because the engine's context is never shared.
2. `WidgetsMainWindow` docks the viewport next to the scene dashboard, inspector, object list, statistics and log widgets as panels the user can rearrange, resize, float, or close. By default the right column holds the **Scene Dashboard** (`sceneDashboardDock`, scene-wide controls) above the **Inspector** (the selected object's fields); both are `InspectorWidget`s, which show a muted `QLabel#inspectorEmptyLabel` placeholder (`setEmptyText`) when they have no editors. `QSettings` persists the arrangement across sessions; `saveState`/`restoreState` pass `kLayoutStateVersion`, which is bumped whenever the default dock set changes so an older saved layout is discarded once.
3. The window constructor ends with `QTimer::singleShot(0, ...)`, which calls `OpenGLViewportWidget::StartEngine()` once the event loop is running. `StartEngine` is deliberately not tied to the viewport's `showEvent`, for two reasons. A restored layout may keep the viewport dock hidden while the inspector and log still need the engine. And the offscreen surface must be created after the event loop is running.
4. `StartEngine()` (GUI thread) runs these steps:
   1. Create the `QtGraphicsContextProvider` and the `Engine`.
   2. Register an `OnFrameReady` callback that queues `pullLatestFrame()`.
   3. `Engine::Start(provider)`, which blocks until the engine thread has a context, renderer and scene.
   4. Apply the fill color, then `CreateScreen` and `SetScreenSize` in device pixels.
   5. Attach `QTSceneInspector`/`QTEngineSettingsAdapter`/`QTSceneDashboardAdapter` to the engine's services and emit `engineInitialized`. The window attaches the log widget to `logService()` from this signal.
5. Each time the engine publishes a frame, `pullLatestFrame()` runs on the GUI thread:
   1. It calls `Engine::GetScreenFrame`, a buffer swap with no copy.
   2. It wraps the pixels in a `QImage` (`Format_RGBX8888`). The frame is already top-down, and RGBX keeps alpha from bleeding through under `WA_OpaquePaintEvent`.
   3. It records `RenderStatistics` from the frame's `fps`/`frameTimeMs`.
   4. It calls `update()`.

   `paintEvent` then draws the image scaled to `rect()`, which covers frames still rendered at the old size during a resize. Without a frame, it fills with the theme fill color.
6. `resizeEvent` and `QEvent::DevicePixelRatioChange` call `SetScreenSize`. The second covers a floating dock moved to a monitor with a different scale. Key, mouse and wheel events go straight to the engine's thread-safe input methods. Ctrl-click picking uses the event's own `modifiers()` and `Engine::ScreenPointToRay`, then selects through `QTSceneInspector`.
7. `QTSceneInspector` adapts engine-neutral inspection fields into Qt field widgets. Qt edits invoke the engine field setters, which the engine serializes with its tick. Engine state remains owned by the engine DLL. Nothing is selected by default: attaching the service leaves the selection empty, and when the selected provider disappears (provider list changed) the selection is cleared rather than moved to another object, so the Inspector shows its placeholder until the user picks an object in the Objects panel or Ctrl-clicks it in the viewport. The scene itself is not in the provider list; its fields live in the Scene Dashboard.
8. `QTSceneDashboardAdapter` (shaped like `QTEngineSettingsAdapter`) adapts the scene's dashboard fields the same way and refetches them on `OnFieldsChanged` (scene created, or the scene's provider generation bumped, e.g. after preprocessing completes).

## Graphics context provider

`QtGraphicsContextProvider` (`ui/include/rendering/QtGraphicsContextProvider.h`) implements the engine's `IGraphicsContextProvider`. The engine calls each of its methods on the engine thread.

- **Constructor (GUI thread).** It captures the default `QSurfaceFormat` and creates the `QOffscreenSurface`, because Qt requires surfaces to be created on the GUI thread. It also logs a warning when `QOpenGLContext::supportsThreadedOpenGL()` is false.
- **Context (engine thread).** `CreateContext`/`MakeCurrent`/`DoneCurrent`/`DestroyContext` create, bind, release and destroy a `QOpenGLContext` on the engine thread. The context is never shared.
- **`GetProcAddress`.** It tries `opengl32.dll` first and then `QOpenGLContext::getProcAddress`. The order matters because `wglGetProcAddress` returns null for GL 1.1 entry points.
- **Destruction (GUI thread).** The provider is destroyed only after the engine thread has been joined.

## Callback marshalling

Engine callbacks fire on the engine thread, with no engine lock held. They must not block on the GUI thread. Every UI subscriber therefore re-posts the work to its own (GUI) thread with `QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection)`. Each subscriber coalesces bursts through an `std::atomic<bool>` pending flag: the engine thread sets it, and the queued handler clears it on the GUI thread.

| Callback | Subscriber | Pending flag |
|---|---|---|
| `Engine::OnFrameReady` | `OpenGLViewportWidget` -> `pullLatestFrame()` | `framePullPending` |
| `IInspectionService::OnProvidersChanged` | `QTSceneInspector` -> `HandleQueuedProvidersChanged()` | `providersChangedPending` |
| `ISettingsService::OnSettingsChanged` | `QTEngineSettingsAdapter` -> `HandleQueuedSettingsChanged()` | `settingsChangedPending` |
| `ISceneDashboardService::OnFieldsChanged` | `QTSceneDashboardAdapter` -> `HandleQueuedSceneDashboardChanged()` | `fieldsChangedPending` |
| `ILogService::OnEntriesChanged` | `EngineLogWidget` -> `handleEntriesChanged()` | `entriesChangedPending` |

Queued handlers re-check that their service (or engine/screen) is still attached, because a detach may happen while a call is queued. The inspector, settings and scene dashboard adapters also keep their own 33 ms sync timers for live field values. Those timers call the services synchronously, and the engine's locking decorators make that safe.

## Shutdown order

`WidgetsMainWindow::closeEvent` runs these steps in order:

1. `saveLayoutState()`.
2. `viewportWidget->ShutdownEngine()`.
3. `QMainWindow::closeEvent`.

`ShutdownEngine()` is idempotent, and it runs these steps in order:

1. Stop the adapters' sync timers.
2. Detach the services: `SetInspectionService(nullptr)`, `SetSettingsService(nullptr)`, `SetSceneDashboardService(nullptr)`, then emit `engineShuttingDown`. On that signal the window calls `engineLogWidget->SetLogService(nullptr)`.
3. `DestroyScreen`.
4. `Engine::Stop()`. This joins the engine thread, which cancels and joins preprocessing jobs and tears down the GL context.
5. Reset the context provider. This destroys the `QOffscreenSurface` on the GUI thread, after the join.

The viewport's destructor also calls `ShutdownEngine()`. Members are destroyed in reverse declaration order, so without that call the adapters would die while the engine thread could still reach them.

## Engine Boundary

The UI includes only the engine's public headers, which live under `engine/include/public/` (the only engine include dir `app_qt` receives) and are included without a layer prefix: `Engine.h`, `graphics/{IGraphicsContextProvider,ScreenFrame}.h`, `inspection/*`, `log/*`, `settings/ISettingsService.h`, `dashboard/ISceneDashboardService.h`. It never includes engine-private headers (`Scene`, `ForwardRenderer`, `InputState`, ...), `QOpenGLWidget`/`QOpenGLFunctions`, or glad.

The boundary responsibilities are:

- The UI owns the Qt application lifecycle, event handling, widget presentation, signal routing, and the Qt-side context provider.
- The engine owns its render thread, the scene, the OpenGL context's use and resource state, rendering decisions, and neutral inspection state.
- The UI forwards Qt input as engine input calls, which are queued and replayed by the engine.
- The UI translates engine `InspectValue` variants into Qt `QVariant` values and widget controls.
- CMake copies engine shaders/assets into the executable output directory. The UI launches with that directory as its working directory.

See [engine architecture](../../engine/docs/architecture.md) for the engine-side threading guarantees.

## Architecture Assessment

### Single Responsibility

Strengths:

- `WidgetsMainWindow` coordinates the top-level composition and signal wiring.
- `QTSceneInspector` owns the Qt-specific inspection bridge rather than placing Qt objects in engine types.
- `QtGraphicsContextProvider` isolates all Qt OpenGL context handling.
- Individual field widgets focus on one presentation/control type.

Weaknesses:

- `OpenGLViewportWidget` combines Qt event handling, engine lifecycle (start/shutdown), frame display, input translation, adapter ownership, and statistics collection.
- `WidgetsMainWindow` combines layout construction, theme state, selection synchronization, and adapter signal routing.

### Open/Closed

The Qt field-widget family and the engine inspection field types provide extension points for new inspector controls. Scene creation is engine-side and concrete, so specialized runtime scenes require engine changes rather than UI changes.

### Liskov Substitution

Qt widgets are consumed through `IInspectWidget`. The viewport is a plain `QWidget` that displays CPU frames, so any engine host that implements `IGraphicsContextProvider` can follow the same pattern. The code assumes concrete inspection widgets provide compatible metadata/value behavior; that contract is structural rather than formally validated.

### Interface Segregation

`IInspectWidget` presents a focused field contract for Qt inspector controls. `QTSceneInspector` exposes a broader collection of selection, visibility, field, and signal responsibilities, because several widgets use it as their adapter boundary.

### Dependency Inversion

The UI depends on the `Engine` facade and on neutral inspection, settings, scene dashboard and log contracts rather than on engine internals. The engine depends on the host only through `IGraphicsContextProvider`. The UI remains directly coupled to Qt Widgets and to Qt's OpenGL context classes inside the provider.

## Source Map

- Target ownership: `CMakeLists.txt`
- Application startup: `ui/src/app/main.cpp`
- Main window composition, engine start/shutdown wiring: `ui/include/windows/WidgetsMainWindow.h`, `ui/src/windows/WidgetsMainWindow.cpp`
- Viewport (frame display, input, engine ownership): `ui/include/widgets/OpenGLViewportWidget.h`, `ui/src/widgets/OpenGLViewportWidget.cpp`
- Graphics context provider: `ui/include/rendering/QtGraphicsContextProvider.h`, `ui/src/rendering/QtGraphicsContextProvider.cpp`
- Engine/Qt inspection bridge: `ui/include/qt-adapters/QTSceneInspector.h`, `ui/src/qt-adapters/QTSceneInspector.cpp` (selection/query logic in `InspectionSession`)
- Settings bridge: `ui/include/qt-adapters/QTEngineSettingsAdapter.h`, `ui/src/qt-adapters/QTEngineSettingsAdapter.cpp`
- Scene dashboard bridge: `ui/include/qt-adapters/QTSceneDashboardAdapter.h`, `ui/src/qt-adapters/QTSceneDashboardAdapter.cpp`
- Engine log panel: `ui/include/widgets/EngineLogWidget.h`, `ui/src/widgets/EngineLogWidget.cpp`
- Field presentation: `ui/include/widgets/inspect_fields/`
- Object-list and selection presentation: `ui/include/widgets/SceneObjectListWidget.h`, `ui/src/widgets/SceneObjectListWidget.cpp`
