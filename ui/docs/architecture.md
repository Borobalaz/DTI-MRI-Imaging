# UI Architecture

## Purpose

The UI is the `app_qt` CMake executable. It owns Qt application startup, the main window, the OpenGL viewport, inspector presentation, object-list controls, styles, and Qt-to-engine adaptation. Its implementation is under `ui/src`; public UI headers are under `ui/include/ui`.

Primary build evidence: `CMakeLists.txt` (`add_executable(app_qt ...)`). The application entry point is `ui/src/app/main.cpp`.

## Top-Down Decomposition

| Module | Role | Main inputs | Main outputs | Ownership |
|---|---|---|---|---|
| Application entry | Configures Qt and the default OpenGL surface format | Process arguments and Qt runtime | `QApplication`, `WidgetsMainWindow`, event loop | Qt owns the application loop; `main` owns the top-level window |
| Main window | Composes the main layout and coordinates UI signals | Viewport, inspector, object list, stats widgets | Window layout and cross-widget synchronization | Window owns its child widgets through Qt parent ownership/raw child pointers |
| OpenGL viewport | Bridges Qt's `QOpenGLWidget` lifecycle to the engine frame loop | Qt input events, OpenGL context, timer ticks | Engine scene updates/renders and render statistics | Viewport owns the scene, renderer helpers, inspector adapter, and timers |
| Scene inspector adapter | Converts engine inspection providers and fields into Qt objects/signals | Engine `InspectProvider`, `InspectField`, ray-selection requests | `QObjectList`, QVariant metadata/values, Qt signals | Adapter owns Qt field wrappers and snapshots; engine owns provider state |
| Inspector widgets | Presents editable fields and sends user edits back through callbacks | Qt field objects and QVariant values | Qt controls and field mutation callbacks | Qt parent/widget ownership controls visual lifetime |
| Object list | Displays selectable engine inspection providers and visibility controls | Provider list and selection/visibility signals | Selection and visibility requests | Main window owns the list widget |
| Styles and statistics | Applies theme resources and displays frame metrics | Theme toggle events, `RenderStatistics` | Qt stylesheets and labels | Window and widgets own presentation state |

## Runtime Flow

1. `ui/src/app/main.cpp` configures a 3.3 core OpenGL surface and creates `QApplication`.
2. `WidgetsMainWindow` creates and lays out the viewport, inspector, object list, and statistics widgets.
3. `OpenGLViewportWidget::initializeGL` receives the current Qt context and calls the engine's `InitializeEngineOpenGL` entry point.
4. The viewport constructs and initializes an engine `Scene`, registers render extractors, attaches camera movement, and gives the scene's providers to `QTSceneInspector`.
5. On each `paintGL`, the viewport synchronizes inspector providers, updates camera/scene state, builds a render frame from typed render commands, invokes its renderer, and publishes frame statistics.
6. `QTSceneInspector` adapts engine-neutral inspection fields into Qt field widgets. Qt edits invoke engine field setters; engine state remains owned by the engine DLL.
7. Signals from the adapter update the object list and inspector without making the engine depend on Qt.

## Engine Boundary

The UI depends on the engine target through public engine headers such as `Scene`, `ForwardRenderer`, `ExtractionRegistry`, `RenderFrameBuilder`, `InputState`, and `InspectField`.

The boundary responsibilities are:

- UI owns Qt application lifecycle, event handling, widget presentation, and signal routing.
- Engine owns scene state, OpenGL resource state, rendering decisions, and neutral inspection state.
- UI translates Qt input into engine `InputState` values.
- UI translates engine `InspectValue` variants into Qt `QVariant` values and widget controls.
- CMake copies engine shaders/assets into the executable output directory; the UI launches with that directory as its working directory.

## Architecture Assessment

### Single Responsibility

Strengths:

- `WidgetsMainWindow` coordinates the top-level composition and signal wiring.
- `QTSceneInspector` owns the Qt-specific inspection bridge rather than placing Qt objects in engine types.
- Individual field widgets focus on one presentation/control type.

Weaknesses:

- `OpenGLViewportWidget` combines Qt event handling, engine scene lifecycle, frame timing, rendering, input translation, inspection synchronization, and statistics collection.
- `WidgetsMainWindow` combines layout construction, theme state, selection synchronization, and adapter signal routing.

### Open/Closed

The Qt field-widget family and the engine inspection field types provide extension points for new inspector controls. The viewport's scene creation and renderer composition are concrete, so specialized runtime scenes require changes or subclassing around the existing viewport path.

### Liskov Substitution

Qt widgets are consumed through `IInspectWidget`, and render viewport behavior follows `QOpenGLWidget` lifecycle overrides. The code assumes concrete inspection widgets provide compatible metadata/value behavior; that contract is structural rather than formally validated.

### Interface Segregation

`IInspectWidget` presents a focused field contract for Qt inspector controls. `QTSceneInspector` exposes a broader collection of selection, visibility, field, and signal responsibilities because it is the adapter boundary used by several widgets.

### Dependency Inversion

The UI depends on engine abstractions and neutral inspection contracts rather than engine internals for field editing. It remains directly coupled to Qt Widgets, Qt OpenGL context behavior, and concrete engine renderer/frame-builder types in `OpenGLViewportWidget`.

## Source Map

- Target ownership: `CMakeLists.txt`
- Application startup: `ui/src/app/main.cpp`
- Main window composition: `ui/include/ui/windows/WidgetsMainWindow.h`, `ui/src/windows/WidgetsMainWindow.cpp`
- Viewport lifecycle and frame loop: `ui/include/ui/widgets/OpenGLViewportWidget.h`, `ui/src/widgets/OpenGLViewportWidget.cpp`
- Engine/Qt inspection bridge: `ui/include/ui/qt-adapters/QTSceneInspector.h`, `ui/src/qt-adapters/QTSceneInspector.cpp`
- Field presentation: `ui/include/ui/widgets/inspect_fields/`
- Object-list and selection presentation: `ui/include/ui/widgets/SceneObjectListWidget.h`, `ui/src/widgets/SceneObjectListWidget.cpp`
