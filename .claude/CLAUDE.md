# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

C++17 Qt 6 Widgets + OpenGL application for DTI/MRI visualization (diffusion tensor imaging, tractography, volume rendering). Built with CMake, dependencies via vcpkg.

Two shipped CMake targets, one process:

- `engine` — Qt-independent `SHARED` library: scene graph, geometry, volumes, input, inspection contracts. Privately links two internal `STATIC` targets:
  - `engine_render` (`engine/include/render/` + `engine/src/render/`) — the render layer: GPU resources, renderer, passes. The only code allowed to make graphics-API calls.
  - `engine_foundation` (`engine/include/foundation/` + `engine/src/foundation/`) — dependency-free utilities shared by both (`EngineLog`, `StableTypeId`).
- `app_qt` — Qt Widgets executable: application shell, viewport, inspector, object list, styling.

`app_qt` links `engine`; both `engine.dll` and `app_qt.exe` land in the same build output directory so the exe can load the engine at runtime. Shaders (`engine/shaders`) and assets (`engine/assets`) are copied next to the executable post-build (see `CMakeLists.txt`).

## Build / Run Commands

All scripts are PowerShell, run from repo root, and read machine-specific paths from `settings.json` (vcpkg root, Qt root/candidates, compiler path). Update `settings.json` when setting up on a new machine, or set `$env:QT_ROOT` / `$env:VCPKG_ROOT` to override.

```powershell
./build.ps1              # configure + build, default config from settings.json (Release)
./build.ps1 Debug
./build.ps1 Release
./build-and-run.ps1       # build then run
./debug.ps1 Release       # run only (assumes already built); also runs windeployqt and copies vcpkg DLLs next to the exe
./clean.ps1               # removes the build directory
```

Manual CMake equivalent:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -DCMAKE_PREFIX_PATH="$env:QT_ROOT/lib/cmake" -DQt6_DIR="$env:QT_ROOT/lib/cmake/Qt6"
cmake --build build --config Release
```

Required dependencies (`vcpkg install ... glm glad assimp stb nlohmann-json`, plus Qt6 Core/Gui/Widgets/OpenGL/OpenGLWidgets/Svg): see `CMakeLists.txt`'s `find_package` calls. ITK (`ITKCommon`, `ITKIOImageBase`, `ITKIONIFTI`, `ITKIONRRD`, `ITKIOMeta`, `ITKIOGDCM`) is a `QUIET` optional dependency gated by `CONNECTOMICS_ENABLE_ITK_IO`; code that needs ITK I/O must guard with that macro since it may not be present.

There is no automated test suite in this repo (no test target, no test framework dependency), and no CI workflow currently checked in (no `.github/workflows/`).

## Architecture

### Engine/UI boundary is deliberately narrow

`engine`'s public headers (`CMakeLists.txt`: `target_include_directories(engine PUBLIC engine/include/public)`) are just the ones under `engine/include/public/`: `Engine.h`, `graphics/`, `inspection/`, `log/`, `settings/`. Everything under `engine/src/include/**` (Scene, Renderer/RenderProxy, RenderCore extraction, Camera, Volume, Light, concrete Geometry, Input, …) is `PRIVATE` to the `engine` target and **not visible to `app_qt`**. The same goes for the render layer's headers (`engine/include/render/**`), which `engine` links privately.

`Engine.h` is explicit about this: *"Public façade for the engine library. This is the only class UI code should depend on; all other engine headers are private implementation details."* In practice `ui/include/widgets/OpenGLViewportWidget.h` includes only `Engine.h` (plus Qt headers) — confirm this when adding new engine functionality: expose it through `Engine`'s facade methods/structs rather than reaching into engine-private headers from UI code.

Crossing the boundary happens through:
- `Engine` facade methods:
  - **Threaded mode** (used by `app_qt`): `Start(IGraphicsContextProvider)`, `Stop`, `IsRunning`, `SetPaused`. The engine owns its render thread, and the host supplies the GL context through the provider (`engine/include/public/graphics/`).
  - **Screens**: `CreateScreen`/`DestroyScreen`/`SetScreenSize`, `GetScreenFrame` (CPU `ScreenFrame`, RGBA8 top-down, buffer swap) and `OnFrameReady`.
  - **Sync mode**, for hosts that drive the loop themselves: `InitializeOpenGL`, `CreateScene`, `Update`, `Render`, `ShutdownGraphics`.
  - **Thread-safe in both modes**: input (`OnKeyChanged`, `OnMouseButtonChanged`, `OnMousePositionChanged`, `OnScroll`, which are queued and replayed at the next tick), `SetViewportSize`/`SetFillColor`, picking (`ScreenPointToRay`), and the `Get*Service` accessors.
  - Callbacks fire on the engine thread with no engine lock held, so hosts must marshal them asynchronously. The per-method guarantees are in `engine/docs/architecture.md`.
- The **inspection contract** (`IInspectionService`, `InspectField`, `InspectHandle`): a Qt-free way for the engine to describe editable scene state. `ui/src/qt-adapters/QTSceneInspector.cpp` adapts these neutral fields into Qt widgets (`ui/include/widgets/inspect_fields/`) and routes edits back through field setters/callbacks.

> The root `docs/` folder (an old ImGui+GLFW prototype's docs) has been removed. `engine/docs/architecture.md`, `engine/docs/render.md` and `ui/docs/architecture.md` are the maintained sources of truth for cross-boundary/subsystem architecture.

### Render layer (`engine/include/render/` + `engine/src/render/`, target `engine_render`)

The only place graphics-API calls may happen; see `engine/docs/render.md`.
- **Layout**:
  - `engine/include/render/**` (minus `internal/`) is the layer's API, visible to `engine`/`core` via the `render/` include prefix. It covers `Shader`, `texture/`, `TextureFormat`, the `geometry/` base + `InstanceBuffer`, `Material`, `VolumeTextureSet`, the `uniform/` contract, the `core/` render commands/frame/data store, and the `renderer/` (`Renderer`/`ForwardRenderer`/`IRenderPass`) and `postprocessing/` (`IPostProcessingPass`) interfaces.
  - `engine/include/render/internal/**` holds the layer-internal headers (`RenderBackend.h`, `opengl/` (`Framebuffer`, `TextureGL.h`, `GpuDeletionQueue`, `FullscreenTriangleGeometry`), `passes/`, `postprocessing/`), included only from render-layer files. `engine/src/render/**` holds the sources: `RenderBackend.cpp`, `opengl/` (the GL implementations, `GraphicsApi.cpp`), `passes/`, `postprocessing/`, `renderer/` and `uniform/`.
- **Access gate (friend gateway)**: GPU-only members are private, and `RenderBackend` (`engine/include/render/internal/RenderBackend.h`, static forwarders: `Use`, `Uniforms`, `Bind`, `Handle`, `Draw`, `DrawInstanced`) is their only friend. Those members are `Shader::Use`/setters/`programId`, `Texture::Bind`/`GetId`, `Geometry::Draw`/`DrawInstanced`/`VAO`, and `InstanceBuffer::GetHandle`. Don't add other friends or widen access; route new needs through `RenderBackend`.
- **Scene code stays GL-free**:
  - `UniformProvider::Apply(IUniformSink&)` writes uniforms, and `IUniformSink::SetTexture(name, texture, unit)` binds textures.
  - Texture formats use `TextureFormat`, never GL enums.
  - `glad` may only be included under `engine/src/render/` (and the `engine/include/render/internal/opengl/` headers). Note that this is held by convention plus grep, not the build, because vcpkg's shared include dir makes `<glad/glad.h>` resolvable from `engine` too.

### Engine internals (private, under `engine/src/include` + `engine/src`)

- **Runtime/threading**: `EngineRuntime` (`engine/src/include/Runtime/`, the pimpl behind `Engine`) owns the engine thread.
  - Each tick, under a coarse `recursive_mutex` (`sceneMutex`), it drains the `InputEventQueue`, runs `Update`, then for each screen runs snapshot -> build -> `Draw` into that screen's `RenderTarget`.
  - Outside the lock it reads back, publishes frames, fires callbacks, and paces to `engine.maxFramerate`.
  - `LockedInspectionService`/`LockedSettingsService` serialize host calls with the tick.
  - `Scene` bumps `providerGeneration` whenever providers can be released, so stale inspection fields go inert.
- **Scene** coordinates camera, lights, drawables/updateables, shader registry, input, and inspection providers; it also constructs default scene content. It builds a `SceneSnapshot` of typed render commands per frame.
- **Render pipeline**: `RenderFrameBuilder` turns a `SceneSnapshot` into a `RenderFrame`/`RenderDataStore` via a registry of `IRenderExtractor`s. `ForwardRenderer` (the current `Renderer` implementation) runs a vector of `IRenderPass`es in registration order — currently `MeshGeometryPass`, `InstancedMeshGeometryPass`, `VolumePass`, `SkyboxPass` (`engine/src/render/passes`). New render features (e.g. particles) add a typed render command (`engine/include/render/core/RenderCommands.h`) + a new pass in `engine/src/render/passes` (header in `engine/include/render/internal/passes`) registered via `Renderer::AddRenderPass`, without touching existing passes. GPU resources upload lazily on the GL thread: `Geometry` at first draw, and `Texture3D` from a `TextureData3D` at first bind. Their destructors enqueue handles into a deletion queue, which is flushed by `CollectGraphicsGarbage()`, so resources may be built and released off the GL thread.
- **Postprocessing**: when any postprocessing passes are registered, `ForwardRenderer` renders the scene passes into an offscreen `sceneFramebuffer` instead of the default framebuffer, then runs the registered `IPostProcessingPass` chain (ping-ponging between two scratch `Framebuffer`s) before blitting to the original target framebuffer. Passes implement `IPostProcessingPass` (`engine/include/render/postprocessing/`; implementations in `engine/src/render/postprocessing/`, headers in `engine/include/render/internal/postprocessing/`) and never bind a framebuffer or set the viewport themselves — that's the orchestrator's (`Renderer::ExecutePostProcessingPasses`) job. Currently one pass is registered: `AntiAliasingPass` (FXAA-style), added via `Renderer::AddPostProcessingPass` in `ForwardRenderer`'s constructor.
- **Instancing**: `InstanceBuffer` carries a mandatory transform stream (locations 4-7) plus optional descriptor-declared attribute streams (locations 8-15, divisor 1) and buffer-texture streams (`samplerBuffer` indexed by `gl_InstanceID`, units 8-11). All of it uploads lazily. `Geometry::DrawInstanced` disables the instance locations after each draw, because the VAO is shared. Details are in `engine/docs/render.md`.
- **SH glyphs**: `SHGlyphField` (`engine/src/include/Glyphs/`) renders an `SHVolume` as instanced icosphere glyphs, displaced in `shaders/sh_glyph/` from per-instance SH coefficients and a per-vertex basis table (`SphericalHarmonics/RealSH.h`, MRtrix3/tournier07, lmax 8). It uses the generic instanced pass, and stride/threshold/subdivision rebuilds run on a `BackgroundTask`. `DtiVolumeScene` builds one from the DTI result's `shVolume`. See `engine/docs/architecture.md`.
- **Volume hierarchy**: abstract `Volume` (owns transform, `VolumeGeometry`, `Shader`, `VolumeTextureSet`; implements `UniformProvider`, `IDrawable`, `InspectProvider`) has two concrete subclasses: `FloatVolume` (single scalar channel) and `DTIVolume` (16 `VolumeData` channels, with no mask: 6 tensor components Dxx/Dyy/Dzz/Dxy/Dxz/Dyz, 3 eigenvector components, 3 eigenvalues L1-L3, and 4 scalars FA/MD/AD/RD).
  - The channels are packed into 5 3D textures: 4 RGB32F + 1 RGBA32F.
  - The packing happens off the GL thread, through `PackDTIVolumeTextures` -> `DTIVolumeTextureData`.
  - The volume is built from those textures plus pre-built render modes (`DTIVolume::CreateRenderModes`). Its ctor makes no GL calls.
  - It has selectable render modes and a Z-slice parameter. See `engine/docs/architecture.md`/`architecture.mmd` for diagrams.
- Everything renderable/updatable implements `UniformProvider` (writes uniforms to an `IUniformSink`; only the render layer can obtain a `Shader` as a sink, via `RenderBackend::Uniforms`) and, where applicable, `IDrawable`/`IUpdateable`/`InspectProvider`.

### Staged MRI preprocessing pipeline (`core/`)

`core` is a generic C++ staged MRI preprocessing pipeline (not Qt- or engine-dependent) with pluggable, model-specific reconstruction stages. Pattern: `MriPreprocessingPipeline` holds an ordered `vector<unique_ptr<IMriPreprocessingStage>>`; each stage implements `Name()`/`Execute(MriPreprocessingContext&)` and mutates a shared context. DTI (diffusion tensor imaging) is the first concrete reconstruction model implemented: `MriToDtiPreprocessor` assembles the DTI stage list and turns raw DWI/bval/bvec input into `DTIVolumeChannels` (exposed on the pipeline's result/context as `dtiChannels`/`outputDtiChannels`) plus surface/streamline meshes; `MriPreprocessingRunner` wraps construction + execution + writing results to disk. fODF (fiber orientation distribution function) reconstruction is planned as a second, parallel model.

Stages that are generic (not tied to any reconstruction model) stay flat under `core/include/Preprocessing/stages/` / `core/src/preprocessing/stages/`: input validation, gradient normalization (bval/bvec), brain surface mesh (isosurface), and the [0,1] channel normalization pass. Stages specific to a reconstruction model live in a model-named subfolder — DTI's under `core/include/Preprocessing/stages/dti/` / `core/src/preprocessing/stages/dti/` (`DtiTensorSynthesisStage`, `DtiPrincipalEigenvectorStage`, `DtiScalarSynthesisStage`, `DtiTensorShStage`, `DtiFiberTractographyStage`); a future fODF model would get its own `stages/fodf/` sibling. Factory declarations follow the same split: `core/include/Preprocessing/stages/CommonPreprocessingStages.h` for generic factories, `core/include/Preprocessing/stages/dti/DtiPreprocessingStages.h` for DTI's. Concrete preprocessor assemblers live under `core/include/Preprocessing/preprocessors/` / `core/src/preprocessing/preprocessors/` (currently `MriToDtiPreprocessor`; a future `MriToFodfPreprocessor` would be a sibling).

`MriPreprocessingPipeline::Execute(request, MriPreprocessingExecutionOptions)` (and `MriToDtiPreprocessor::Process`) takes a `CancellationToken` plus an `onProgress` callback that fires on the calling thread. It checks for cancellation and reports progress between stages. Long-running stages also call `MriPreprocessingContext::ThrowIfCancelled`/`ReportStageFraction` internally. A cancel throws `MriPreprocessingCancelled`, which is reported as a cancellation, not a failure. `DtiVolumeScene` runs Run/Load/Save as `BackgroundTask` jobs on a worker thread (latest-wins for Run/Load, Save refused while busy). The worker also packs the volume textures, and only the cheap apply runs on the engine thread, using a shader cache that is compiled once.

Stage order: input validation → gradient normalization (bval/bvec) → tensor synthesis (least-squares fit of the 3×3 diffusion tensor) → principal eigenvector/eigenvalue extraction → scalar synthesis (FA/MD/AD/RD) → tensor ODF to spherical harmonics (lmax 8 fit of the raw tensor ODF into `MriPreprocessingResult::shVolume` with raw FA as mask; must precede normalization, live runs only, not persisted) → brain surface mesh (isosurface) → fiber tractography (seed at FA > threshold, trace along principal eigenvector, stop at FA/eigenvalue thresholds, build tube mesh) → normalization (scale channels to [0,1]).

Tractography parameters live in `MriTractographySettings` (FA seed/stop thresholds, step size in voxels, max steps, seed stride/max seeds, tube radius/segments) and it implements `InspectProvider` so its fields are editable through the same inspection mechanism as scene objects.

To add a stage: implement `IMriPreprocessingStage`, add a factory function (in the generic or model-specific factory header as appropriate), and register it via `AddStage()` in the relevant preprocessor's constructor (e.g. `MriToDtiPreprocessor`).

Saved preprocessing datasets (payload files + a `metadata.json` provenance/parameters manifest) are read/written through `core/include/Preprocessing/io/` — a registry (`PreprocessingDatasetFormatRegistry`) dispatches `IPreprocessingDatasetWriter`/`IPreprocessingDatasetReader` implementations by a `preprocessorType` string, so each reconstruction model's save/load format is independent. DTI's implementation lives under `io/dti/` (`DtiPreprocessingDatasetWriter`/`Reader`, plus `DtiPreprocessingParameters` for provenance JSON, and `DtiChannelTable` for the shared name↔channel-member lookup table both the writer and reader walk); to add a new preprocessor's saved-dataset support (e.g. a future fODF model), implement the two interfaces for it and register both inside `GetDefaultPreprocessingDatasetFormatRegistry()` (`core/src/preprocessing/io/PreprocessingDatasetFormats.cpp`) — no other file under `Preprocessing/io` needs to change. `DtiVolumeScene` exposes "Save current dataset..."/"Load dataset" actions through the same inspection contract as its other controls.

`DtiVolumeScene` (`core/include/DtiVolumeScene.h`) is the concrete DTI scene builder; it depends on engine-private `Scene`/`GameObject` types, a pre-existing exception to `core`'s engine-independence.

Separately, `scripts/run_hd_bet.py` is a standalone Python helper (HD-BET skull stripping) — not part of the C++ build/pipeline.

### UI layer (`ui/`)

`ui/src/app/main.cpp` sets up a 3.3 core-profile OpenGL surface and `QApplication`, then shows `WidgetsMainWindow` (`ui/src/windows`). `WidgetsMainWindow` docks `OpenGLViewportWidget`, `InspectorWidget`/`InspectProviderWidget` (editable fields), `SceneObjectListWidget`, and `RenderStatisticsWidget` as movable/resizable/floatable `QDockWidget`s (no central widget); dock layout is persisted across sessions via `QSettings`. It also owns a `MainToolBar` (view-menu dock toggles, theme toggle) and a `MainWindowShortcuts` controller, and hot-reloads the active theme's `.qss` on disk changes via a polling `QTimer`. There is a single concrete viewport, `OpenGLViewportWidget`, which is a plain `QWidget` that makes no GL calls. It owns the `Engine` and a `QtGraphicsContextProvider` (`ui/include/rendering/`).
  - `WidgetsMainWindow` calls its `StartEngine()` once via `QTimer::singleShot(0, ...)`, and calls `ShutdownEngine()` from `closeEvent`.
  - It pulls each published `ScreenFrame` into a `QImage` and paints it.
  - It translates Qt input events into `Engine` input calls, and feeds `QTSceneInspector`.
  - Engine callbacks (frame ready, providers/settings/log changed) are marshalled to the GUI thread with queued, coalesced `QMetaObject::invokeMethod`.
  - There is no separate DTI-specific viewport subclass.

Scene inspection is split into two layers: `InspectionSession` (`ui/include/qt-adapters/InspectionSession.h`) is engine-agnostic, Qt-free selection/query logic (select by id/ray, visibility, current fields) built purely on `IInspectionService`; `QTSceneInspector` is a thin Qt adapter that owns an `InspectionSession` plus a sync `QTimer` and converts `InspectField`/`InspectValue` into Qt field widgets (`ui/include/widgets/inspect_fields/`) and `QVariant`s/signals for the rest of the UI.

Theming is a QSS token composer, not a class hierarchy: `StyleSheetComposer` (`ui/include/styles/StyleSheetComposer.h`) discovers every `ui/styles/widgets/**/*.qss` file, substitutes `@token-name` references against `ui/styles/tokens/<dark|light>.ini` (plus theme-independent layout metrics in `tokens/common.ini`), and concatenates the result for `setStyleSheet()`; `ResolveThemeToken`/`ResolveCommonToken` expose individual token values to C++ (`QPalette`, native titlebar, layout calls) that QSS can't reach.

### Documentation conventions

Subsystem docs are deliberately split and owned separately: engine architecture/diagrams live under `engine/docs/*.md` / `*.mmd`, the render layer in `engine/docs/render.md`, UI under `ui/docs/*.md` / `*.mmd`. Cross-boundary behavior is documented on the owning side and linked from the other side — there is no combined root architecture doc. When updating these docs, keep code as ground truth and update docs to match code, not the reverse.

### `.claude/`

Subagents live in `.claude/agents/`, skills in `.claude/skills/`. House rules: keep changes scoped to what was asked (no unrelated refactors), prefer incremental edits over new abstractions, build/run only via the PowerShell scripts and report their failures rather than falling back to raw CMake.

Agents follow a strict planner/implementer split: `cpp-architecture-planner`, `qt-gui-planner`, `rendering-shader-planner`, and `mri-processing-planner` own feature/architecture/UX decisions, produce detailed plans, and validate the built result; `cpp-implementer` and `glsl-implementer` are the *only* agents that write code (all C++, and GLSL respectively) and execute those plans without making design decisions of their own, asking the originating planner when a plan is ambiguous.

This split applies even when a generic workflow (e.g. plan mode's own phase instructions) says to launch a "Plan agent" for the design step — substitute the matching specialized planner above instead (pick by subject area: general C++ → `cpp-architecture-planner`, Qt/UI → `qt-gui-planner`, rendering/shaders → `rendering-shader-planner`, MRI/DTI pipeline → `mri-processing-planner`), never the generic one.

<!-- CODEGRAPH_START -->
## CodeGraph

In repositories indexed by CodeGraph (a `.codegraph/` directory exists at the repo root), reach for it BEFORE grep/find or reading files when you need to understand or locate code:

- **MCP tool** (when available): `codegraph_explore` answers most code questions in one call — the relevant symbols' verbatim source plus the call paths between them, including dynamic-dispatch hops grep can't follow. Name a file or symbol in the query to read its current line-numbered source. If it's listed but deferred, load it by name via tool search.
- **Shell** (always works): `codegraph explore "<symbol names or question>"` prints the same output.

If there is no `.codegraph/` directory, skip CodeGraph entirely — indexing is the user's decision.
<!-- CODEGRAPH_END -->
