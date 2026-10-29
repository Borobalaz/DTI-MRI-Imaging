# Engine Architecture

## Purpose

The engine is the `engine` CMake shared-library target. It owns scene state, input state, volume data, concrete geometries, scene-to-frame extraction, and neutral inspection contracts. The target is built from `engine/src/{public,scene,runtime}` (plus `core/src`), exports its public headers from `engine/include/public`, and does not depend on Qt.

GPU resources (shaders, materials, textures, geometry buffers), the renderer, its passes, and every graphics-API call live in the separate **render layer**: the `engine_render` static target (`engine/include/render/` + `engine/src/render/`). `engine` links it `PRIVATE`. Both link the `engine_foundation` static target (`engine/include/foundation/` + `engine/src/foundation/`: `EngineLog`, `StableTypeId`, and the header-only `foundation/threading/` primitives), so the dependency direction is `engine` -> `engine_render` -> `engine_foundation`. See [the render layer architecture](render.md) for the layer's public/private split and its `RenderBackend` gateway rule.

Primary build evidence: `CMakeLists.txt` (`add_library(engine SHARED ...)`, `add_library(engine_render STATIC ...)`, `add_library(engine_foundation STATIC ...)`). Representative runtime entry points are `engine/include/scene/Scene.h`, `engine/include/scene/extraction/RenderFrameBuilder.h`, and the layer's `engine/include/render/renderer/ForwardRenderer.h`.

## Top-Down Decomposition

| Module | Role | Main inputs | Main outputs | Ownership |
|---|---|---|---|---|
| Scene | Coordinates camera, lights, drawables, updateables, shaders, input, and inspection providers | Input state, scene objects, resources | Scene snapshots, render proxies, inspection providers, scene-wide dashboard fields | Engine owns the scene graph and contained shared objects |
| Camera and input | Maintains view/projection state and movement behavior | `InputState`, elapsed time, movement components | Camera matrices and updated transforms | Camera owns its movement component; UI only supplies input events |
| Geometry and drawable objects | Represents mesh geometry, game objects, volumes, and renderable content | Geometry, materials, transforms, volume data | `RenderProxy` values and updates | Scene owns drawable/updateable shared pointers |
| Resources | Loads models and volume files, and creates render-layer resources (shaders, materials, textures) for scene content | File paths, decoded data | Resource objects; their GPU-side behavior lives in the render layer | Resource objects are shared through engine-owned `std::shared_ptr` values |
| Render core | Converts typed scene render commands into renderer-consumable frame data | `SceneSnapshot`, renderer descriptor, extractor registry | `RenderFrame` and `RenderDataStore` (render-layer types) | Registry owns extractor instances and ordering metadata |
| Renderer (render layer) | Executes a renderer strategy against a built frame | `RenderFrame` | Graphics-API draw calls | Lives in `engine_render`; `EngineRuntime` owns the `ForwardRenderer` instance, created once the graphics API is initialized. See [render layer](render.md) |
| Runtime | Implements the `Engine` facade: engine thread, locks, screens, input queue, service decorators | Host calls from any thread, a host `IGraphicsContextProvider` | Published CPU frames (`ScreenFrame`), change callbacks | `Engine` owns `EngineRuntime` through a pimpl; the runtime owns the scene, renderer, and services |
| Inspection contract | Describes editable engine state without Qt types | Provider state and callbacks | `InspectField` metadata, values, and setters | Engine owns providers and neutral field objects |

## Runtime Flow

The `Engine` facade (`engine/include/public/Engine.h`) runs in one of two modes. Both run the same tick body (`EngineRuntime::UpdateLocked` + `RenderLocked`).

**Threaded mode** (used by `app_qt`):

1. The host calls `Engine::Start(provider)` with an `IGraphicsContextProvider` (`engine/include/public/graphics/IGraphicsContextProvider.h`). `Start` spawns the engine thread and blocks until initialization finishes. On the engine thread: `CreateContext` -> `MakeCurrent` -> `InitializeGraphicsApi` (render layer, resolving entry points through the provider's `GetProcAddress`) -> `ForwardRenderer` -> `DtiVolumeScene`.
2. The host creates one or more screens (`CreateScreen`, `SetScreenSize` in device pixels).
3. Each tick, under the scene lock:
   1. Drain queued input into `InputState`.
   2. Run `Scene::ReloadShadersIfChanged` and `Scene::Update`. For `DtiVolumeScene` this polls and applies preprocessing jobs.
   3. Run `CollectGraphicsGarbage`.
   4. For each screen, resize its `RenderTarget`, set the camera aspect, then `Scene::CreateSnapshot` -> `RenderFrameBuilder::Build` -> `Renderer::Draw(frame, target)`.
4. Then, with no lock held: read each target back into CPU memory, publish it as the screen's newest `ScreenFrame`, fire `OnFrameReady` and the service change notifications, and pace to `engine.maxFramerate` (`0` = uncapped).
5. The host takes frames with `GetScreenFrame` (a buffer swap, no pixel copy) and displays them. It makes no graphics-API calls.
6. `Engine::Stop()` (also called by `~Engine`) joins the engine thread, which tears down in order: scene (cancelling/joining preprocessing jobs) -> screens -> renderer -> remaining GPU garbage -> `ShutdownGraphicsApi` -> `DoneCurrent` -> `DestroyContext`.

**Sync mode** is for hosts that drive the loop themselves on a thread with the context current: `InitializeOpenGL` -> `CreateScene` -> per frame `Update`/`Render` -> `ShutdownGraphics`. `Render` draws into every screen if any exist (and publishes their frames), otherwise into the currently bound framebuffer at the `SetViewportSize` size. Calling a sync-mode method while `IsRunning()` asserts in debug builds and is otherwise ignored with an error log.

In both modes the scene exposes `InspectProvider` instances and neutral `InspectField` values to the UI through `GetInspectionService()`. The scene itself is **not** an inspect provider (it no longer appears in the provider list): its scene-wide controls (`Scene::GetDashboardFields()`, overridden by `DtiVolumeScene` for preprocessing, tractography, fODF, glyph and rotation controls) are exposed as a flat field list through `GetSceneDashboardService()` (`engine/include/public/dashboard/ISceneDashboardService.h`).

The engine resource files are stored under `engine/assets` and `engine/shaders`. CMake copies them beside the UI executable at build time; engine runtime paths therefore remain relative to the deployed application directory.

## Threading Model

| Thread | Owner | Does |
|---|---|---|
| Host (GUI) | the host (`app_qt`'s Qt GUI thread) | Calls `Engine` facade methods and displays published frames. Never touches the graphics API. |
| Engine | `EngineRuntime` (`std::thread`, `Engine::Start`/`Stop`) | The GL thread: input drain, `Update` (including applying finished preprocessing jobs and lazy GPU uploads), render, readback, publish, callbacks, pacing. |
| Preprocessing worker | `BackgroundTask<DatasetJobOutcome>`, owned by `DtiVolumeScene` | `MriToDtiPreprocessor::Process` or `MriToFodfPreprocessor::Process` (chosen by the scene's Reconstruction Model), dataset read/write, DTI volume texture packing. Pure CPU, never the graphics API. |

In sync mode, the host thread calling `Update`/`Render` takes the engine thread's role.

### `Engine` guarantees per method

| Methods | Threading |
|---|---|
| `Start`, `Stop`, `IsRunning`, `SetPaused` | Any thread except the engine thread. `Stop` is idempotent, and calling it from the engine thread (e.g. from a callback) asserts and is ignored. |
| `CreateScreen`, `DestroyScreen`, `SetScreenSize`, `GetScreenFrame`, `OnFrameReady` | Thread-safe. |
| `InitializeOpenGL`, `CreateScene`, `Update`, `Render`, `ShutdownGraphics` | Sync mode only. Call them on the thread that has the context current. |
| `SetViewportSize`, `SetFillColor` | Thread-safe. Applied at the next render. |
| `OnKeyChanged`, `OnMouseButtonChanged`, `OnMousePositionChanged`, `OnScroll` | Thread-safe. Queued and replayed in order at the start of the next tick. |
| `IsKeyDown` | Thread-safe. Reflects every `OnKeyChanged` call so far, including events not yet replayed. |
| `GetInspectionService`, `GetLogService`, `GetSettingsService`, `GetSceneDashboardService` | Thread-safe. The services are valid for the `Engine`'s lifetime, and every call on them (and on the fields they return) is serialized with the tick. |
| `ScreenPointToRay` | Thread-safe. Locks the scene. |

**Callback rule.** Every callback (`OnFrameReady`, `OnProvidersChanged`, `OnEntriesChanged`, `OnSettingsChanged`, `OnFieldsChanged`) fires on the engine thread in threaded mode, or on the thread calling `Update`/`Render` in sync mode. It never fires while an engine lock is held. A callback must not block on the host UI thread and must not call `Stop()`. Hosts marshal asynchronously instead; the Qt host queues, see [UI architecture](../../ui/docs/architecture.md).

### `EngineRuntime` and its locks

`EngineRuntime` (`engine/include/runtime/EngineRuntime.h`) is the pimpl behind `Engine`, so the public header stays free of `<thread>`/`<mutex>`. Its locks:

- **`sceneMutex`** (`std::recursive_mutex`) guards the scene, renderer, frame builder, `InputState`, and the scene inspection and dashboard services. A tick holds it for input drain + `Update` + snapshot/`Draw`. `Draw` has to stay inside, because render commands reference live `UniformProvider`s. Readback, publishing, callbacks and pacing run outside it, and preprocessing never runs under it.
- **`screensMutex`** guards the screen map and the requested sizes.
- **Per-`ScreenSlot` `frameMutex`** guards only that screen's published (`ready`) frame. The slot's `RenderTarget` and back buffer belong to the GL thread.
- **Lock order** is `sceneMutex` -> `screensMutex` -> `frameMutex`. Callbacks run with no lock held.

The context provider of the running engine thread is held in a `thread_local`, and the graphics-API loader reaches it through a plain-function trampoline.

Each tick whose locked update + render exceeds 33 ms logs `Slow tick: update <a> ms, render <b> ms` (source `EngineRuntime`).

### Screens

A screen is an engine-owned offscreen `RenderTarget` that is rendered every tick. Its frame is read back as a `ScreenFrame` (`engine/include/public/graphics/ScreenFrame.h`):

- `pixels` is tightly packed RGBA8, with rows **top-down** (row 0 is the top of the image), so hosts never flip.
- The frame also carries `width`, `height`, `frameIndex`, `frameTimeMs` and `fps`.
- `GetScreenFrame` swaps the newest frame into the caller's `ScreenFrame`, which recycles the caller's previous buffer. It returns `false` when there is no newer frame.
- A screen with a non-positive size is not rendered.
- `DestroyScreen` may race with a tick in flight. The slot is kept alive for that tick, and its GPU objects go through the deletion queue.

### Input and service decorators

- **`InputEventQueue`** (`engine/include/runtime/input/InputEventQueue.h`): `Push` from any thread keeps a producer-side keys-down mirror (which backs `IsKeyDown`). `DrainInto(InputState&)` replays the events in push order at the start of a tick, so a press/release within one tick is not lost.
- **`LockedInspectionService`** (`engine/include/runtime/inspection/LockedInspectionService.h`): an `IInspectionService` decorator that is attached to the scene's `SceneInspectionService`. Every call, and every getter/setter of the fields it returns, runs under `sceneMutex`. While no scene is attached, queries return empty results.
- **`LockedSettingsService`** (`engine/include/runtime/settings/LockedSettingsService.h`): the same decorator for `ISettingsService`. Settings are declared once per process, so its wrapped fields never go stale.
- **`LockedSceneDashboardService`** (`engine/include/runtime/dashboard/LockedSceneDashboardService.h`): the same decorator for `ISceneDashboardService`, attached to the scene's `SceneDashboardService` (`engine/include/runtime/dashboard/SceneDashboardService.h`, which returns `Scene::GetDashboardFields()`). Like `LockedInspectionService`, it is attached/detached with the scene and stamps its wrapped fields (see the [provider-generation invariant](#provider-generation-invariant)), because dashboard closures capture the scene's `this`. While no scene is attached, `GetFields` returns an empty list.
- `EngineLogService` has its own mutex, because workers log while the GUI thread reads entries.
- Change detection is split. `PollForProviderChanges` / `PollForEntryChanges` / `PollForSettingsChanges` / `PollForFieldChanges` (dashboard: the scene's provider generation changed) run under the lock and only return a `bool`. The runtime calls each service's `NotifyChanged()` after releasing it.

### Provider-generation invariant

Inspection field closures capture raw provider pointers. `Scene` therefore increments `providerGeneration` in **every** method that can release a provider: `ClearGameObjects`, `ClearLights`, `ClearDrawables`, `ClearVolumes`, `ClearGlyphFields`, `SetSkybox` (when replacing one), `ClearSkybox`, and `RebuildInspectProviders`. `LockedInspectionService::GetFields` (and `LockedSceneDashboardService::GetFields`) stamps each wrapped field with that generation, plus an attach epoch so a re-created scene can't alias an old generation. Once either changes, the field's getter returns its `initialValue` and its setter does nothing. Hosts refetch fields from the providers-changed (or dashboard fields-changed) notification; the same generation bump is what makes the dashboard pick up structural changes such as the Run -> Rerun label. Any new code that removes providers must bump the generation.

### Preprocessing jobs (`DtiVolumeScene`)

`DtiVolumeScene` (`core/include/DtiVolumeScene.h`) runs dataset jobs (`DatasetJobKind::Run`/`Load`/`Save`) as `BackgroundTask<DatasetJobOutcome>`s and never blocks the engine thread on them.

- **Run** snapshots `currentRequest` plus a copy of the tractography and fODF settings and the selected Reconstruction Model, so later inspector edits can't race the worker. The worker calls `MriToDtiPreprocessor::Process(request, options)` or `MriToFodfPreprocessor::Process(request, options)`, with progress and cancellation passed through `MriPreprocessingExecutionOptions`.
- **Load** reads a saved dataset on the worker. Its provenance parameters are applied to the scene at apply time.
- **Run and Load both** finish on the worker by calling `PackDTIVolumeTextures` (`engine/include/scene/volume/DTIVolume.h`). This validates the channels (same dimensions/spacing, finite voxels) and packs them into the five `TextureData3D` payloads of the volume texture contract. Run publishes `Preparing volume textures` while it does this.
- **Save** writes the last applied result together with the applied request/settings, so provenance records what was actually applied. It writes into a staging folder and then renames, so a failed save leaves no partial dataset.
- **Policy.** One job is in flight at a time.
  - Run/Load is latest-wins: a running Run/Load is cancelled and parked in `retiringJob`, so `Update` never joins.
  - A Run/Load requested during a Save waits for it.
  - A Save requested while anything else is in flight is refused with `Busy: save refused`.
  - `Cancel preprocessing` stops a running or queued Run/Load, never a Save.
- **Progress.** Stage progress goes through a per-job `SharedProgress` mailbox. It is shown in the read-only `Status` inspector field as `<stage> (step k/n, NN%)`, `Ready`, `Cancelled` or `Failed: ...`.
- **Apply.** `Update` polls the jobs. A finished Run/Load is applied by `ApplyPreprocessingResult` on the engine thread.
  - It builds a new `DTIVolume` from the pre-packed textures. The ctor makes no GL calls, because each `Texture3D` uploads lazily at its first bind.
  - It builds the surface, streamline and seed `GameObject`s into locals, then commits: it swaps them into the scene and calls `RebuildInspectProviders` once.
  - When the result carries SH data it also builds the `dti_sh_glyphs` [`SHGlyphField`](#sh-glyph-fields) with the volume's rotation: from `fodfShVolume` when present (Glyph Source defaults to fODF), otherwise from `shVolume`. The previous field's Display mode is carried over; stride, threshold and subdivision reset. "dti" datasets carry no SH, so they log `Dataset has no spherical-harmonics data` and show no glyphs; "fodf" datasets carry both.
  - Both the new `DTIVolume` and the new glyph field receive the scene's [slice plane](#slice-plane). The `SliceController` is not re-added: it re-clamps the offset to the new dataset box, takes the cached unlit shader, and resets its rotation to the new objects' base rotation.
  - A failure or a non-finite dataset therefore leaves the previous scene shown.
  - The apply logs `DTI apply: <ms> ms (engine thread)`.
- **Shader cache.** `EnsureShaderCache` compiles the five `DTIVolume` render-mode shaders (`DTIVolume::CreateRenderModes`) and the brain-surface, streamline, seed-point, SH-glyph and unlit (slice plane) shaders once on the first `Update`, and registers them for hot reload. Every apply reuses them, so a rerun compiles nothing, and their stored (inspector-tuned) uniforms persist across reruns. A shader that failed to build is retried at the next apply, because hot reload skips invalid programs.
- **Destruction.** The scene's destructor cancels and joins its jobs. `Engine::Stop` destroys the scene outside `sceneMutex` for this reason.

The pipeline itself (`core/`) stays thread-agnostic: it only exposes progress and cancellation hooks. `MriPreprocessingContext::ThrowIfCancelled`/`ReportStageFraction` are checked inside the long-running stages, and `MriPreprocessingCancelled` is reported as a cancellation, not as a failure. `VolumeFileLoader`/`MeshFileLoader` keep their last-error state `thread_local`, because workers and the engine thread load concurrently.

### Foundation `threading/` primitives

These are header-only, under `engine/include/foundation/threading/`, and Qt- and GL-free:

- **`Cancellation.h`**: `CancellationSource` (`Cancel`, `Token`) and `CancellationToken` (`IsCancellationRequested`, `ThrowIfCancellationRequested`), backed by a shared atomic flag. A default token never cancels. Also defines the `OperationCancelled` exception.
- **`SharedProgress.h`**: a latest-value `ProgressSnapshot{label, step, stepCount, fraction}` mailbox behind a mutex (`Publish`/`Read`).
- **`BackgroundTask.h`**: `BackgroundTask<T>` runs one work function on its own `std::thread`, starting at construction.
  - API: `Cancel`, `IsFinished`, and `TakeResult`, which joins and rethrows a stored exception.
  - The destructor cancels and then joins.
  - The work function and its captures are destroyed on the owning thread, never on the worker.

### SH glyph fields

`SHGlyphField` (`engine/include/scene/glyphs/SHGlyphField.h`) draws an instanced field of spherical-harmonics glyphs. It is a `UniformProvider`, `IDrawable`, `IUpdateable` and `InspectProvider`, registered with `Scene::AddGlyphField` and removed with `ClearGlyphFields`.

- **Input.** An `SHVolume` (`scene/glyphs/SHVolume.h`) holds dense real SH coefficients per voxel (lmax ≤ 8, MRtrix3/tournier07 convention, `VolumeData::FlatIndex` order) and an optional mask volume. It is shared by `shared_ptr<const>`, never copied.
- **SH math.** `scene/spherical_harmonics/RealSH.h` (`namespace RealSH`) provides the basis, the per-vertex basis table (Y_k only, packed 4 per RGBA32F texel and zero-padded to 48 values: 12 texels per vertex, the same packing as the per-instance coefficients), and the least-squares fit matrix. The same code serves the glyph renderer and the DTI fit, so the convention cannot drift between them.
- **Mesh.** An `IcosphereGeometry` of unit radius. Its shared-vertex order is deterministic, so the vertex shader finds its basis row by `gl_VertexID`.
- **Instances.** Built by the pure function `BuildSHGlyphField`. In Display = Volume:
  - voxels are stepped by stride and kept when the mask passes the threshold and the coefficients are non-zero;
  - each glyph gets a transform (Volume-space center, scale `0.5 · stride · min(spacing)/maxExtent`), a location-8 `aInstanceMaxRadius` attribute, and an `instanceCoefficients` buffer stream (RGBA32F, 48 floats);
  - the field's own transform scale stays 1, so anisotropic voxels don't distort glyphs, and with the volume's rotation it overlays the `DTIVolume`.
  - In Display = Slice the stride is ignored (the Stride field is read-only and reads 1): glyphs sit on a regular grid on the [slice plane](#slice-plane) with spacing `min(spacing)/maxExtent`, anchored at the projection of the centre voxel, and their coefficients and mask are trilinearly interpolated (the same sample the volume slice shader takes there). Both modes produce the same instance layout.
- **Rendering.** Through the generic `MeshInstancedRenderCommand`/`InstancedMeshGeometryPass`. The field adds itself to the command's uniforms to bind `shGlyph.basis` (unit 12), `scale`, `normalize` and `coefficientTexelsPerInstance`. `shaders/sh_glyph/sh_glyph_vertex.glsl` evaluates r(n) as 12 vec4 dot products and clamps r ≥ 0. `sh_glyph_fragment.glsl` is a copy of `pbr_fragment.glsl` with direction-RGB albedo and a faceted screen-space normal (`dFdx`/`dFdy` of the world position); there is no `#include`, so PBR lighting changes must be mirrored there. The glyph material sets `Material::SetCullBackFaces(true)`, so back faces are culled (the icosphere is CCW-outward).
- **Rebuilds.** Changes to stride, threshold, subdivision or Display mode run `BuildSHGlyphField` as a `BackgroundTask`. The first build starts in the first `Update()`, so the owner can hand over the slice plane first.
  - Setters only store the new value and cancel the running build.
  - `Update()` takes the finished result, swaps the buffer, mesh and basis into place, and only then starts the next pending build. The engine thread never joins.
  - A superseded result is discarded, and a failed build keeps the old state.
  - Scale and normalize are uniforms, so they apply immediately.
  - `SetVolume` swaps the source `SHVolume` with the same latest-wins rebuild; the mesh and basis table are kept, because they depend only on the subdivision level.
  - In Slice mode `Update()` compares the shared plane's version with the version the last `StartBuild` captured and requests a rebuild when it changed. `StartBuild` copies the plane by value, so the worker never reads the shared object, and the old glyphs stay until the new ones land.
- **DTI source.** `DtiTensorShStage` runs right after scalar synthesis and before `DWINormalizationStage`, which destroys the tensor. It fits the tensor ODF (uᵀD⁻¹u)^(−3/2), sampled on 642 directions, to lmax 8, normalized to unit integral. It publishes `MriPreprocessingResult::shVolume` with the raw FA as the mask. The "dti" dataset format does not persist it; the "fodf" format does.

### Slice plane

`SlicePlane` (`engine/include/scene/slicing/SlicePlane.h`) is the scene's single oblique slicing plane, `dot(normal, p) == offset`.

- **Dataset object space.** The space `SHGlyphField` places glyphs in and `Volume`'s transform scale maps its unit cube into: centred at the origin, box `[-E/2, +E/2]` with `E = DatasetObjectExtents(dimensions, spacing) = dimensions * spacing / max component`, so the longest axis has length 1 ("normalised object units"). Voxel (i,j,k)'s centre is `((ijk + 0.5) / dimensions - 0.5) * E`. Volume's unit cube q maps here by `p = E * q`. Everything `DtiVolumeScene` processes shares this space (up to the common object rotation), so the plane is fixed to the data and rotates with it.
- **Value plus version.** A plain CPU value type: unit normal, offset (clamped to the stored box half-extents along the normal) and a version bumped by every setter that changed something. Consumers hold `shared_ptr<const SlicePlane>` and compare `GetVersion()`. Helpers: `IntersectBox` (ordered convex plane/box polygon, CCW from +normal), `GetInPlaneBasis`, `ToUnitCubeSpace(E)` (`normal' = normalize(E * n)`, `offset' = offset / length(E * n)`).
- **Consumers.**
  - `DTIVolume::Apply` sets `slicePlane.normal`/`slicePlane.offset` in unit-cube object space via `ToUnitCubeSpace(GetObjectExtents())` (intrinsic extents, not the editable transform scale). Only the two slice render modes declare them; with no plane the default is axial through the centre. They are not stored uniforms, because `VolumePass` applies stored uniforms after the providers.
  - `SHGlyphField` in Display = Slice (see [SH glyph fields](#sh-glyph-fields)): version polling in `Update`, a value snapshot in `StartBuild`.
  - `SliceController` (`core/include/SliceController.h`), owned by `DtiVolumeScene`, owns the shared plane and draws it: an always-on outline (`LineLoopGeometry`, `MeshGeometryPass`) and an optional semi-transparent fill (`ConvexPolygonFillGeometry`, `TransparentMeshRenderCommand` -> `TransparentPass`), both with the unlit shader (`material.albedoFactor`/`material.opacity`). Its geometry is rebuilt in its `Update` when the plane version changes, and its transform spins with the dataset. It is registered once in the `DtiVolumeScene` ctor (`AddDrawable`/`AddUpdateable`), which the type-filtered clears never remove, so the plane survives reruns and loads.
- **Inspector metadata.** Field ranges and `readOnly` are frozen when the UI fetches fields, so `DtiVolumeScene::Update` calls `RebuildInspectProviders` when the offset range/step or the glyph Display mode changed (a preset pick, a normal edit, a new dataset box, a Display switch). Offset scrubbing does not trigger a re-fetch.
- **Threading.** The plane is not internally synchronised. Every reader and writer runs under the scene lock: inspector setters through `LockedInspectionService`, `Update`/`Apply`/`BuildRenderProxy` inside the engine tick. Glyph builds only ever see a copy.

## Boundary Contract

The engine-to-UI boundary is intentionally narrow in dependency direction:

- The UI links to the `engine` DLL and includes engine public headers.
- The engine does not include Qt or UI widget headers.
- `InspectProvider`, `InspectField`, and `InspectValue` cross the boundary as neutral C++ contracts.
- The UI converts those contracts into Qt objects in `QTSceneInspector`.
- The UI owns event-loop, widget, signal, and adapter lifetimes.
- In threaded mode the UI makes no graphics-API calls. It supplies an `IGraphicsContextProvider`, and it displays the CPU `ScreenFrame`s the engine publishes.

The current public API uses C++/STL and GLM types across the DLL boundary. This requires the engine and UI to use compatible MSVC, C++ standard, runtime, and dependency configurations.

## Architecture Assessment

### Single Responsibility

Strengths:

- `ExtractionRegistry` focuses on extractor registration and ordered extraction.
- `RenderFrameBuilder` focuses on frame construction.
- `ForwardRenderer` focuses on drawing a built frame.
- `InspectField` separates neutral inspection metadata and mutation callbacks from Qt presentation.

Weaknesses:

- `Scene` combines scene ownership, default content construction, shader management, update orchestration, render-proxy gathering, and inspection-provider discovery.
- `Volume` and several drawable classes combine rendering state with inspection-field construction.
- Render submission uses independently typed `RenderDataStore` channels, consumed by registered `IRenderPass` classes inside the render layer.

### Open/Closed

The extractor registry and `Renderer` abstraction provide extension points for new extraction behavior and renderer implementations. Adding default scene content or new resource behavior still requires changes in concrete scene/resource code.

### Liskov Substitution

`Renderer` and `InspectProvider` define substitution points used by the UI and render flow. The code relies on providers returning valid names/fields and renderers honoring their descriptors; those behavioral expectations are implicit rather than formally documented in code contracts.

### Interface Segregation

`IDrawable`, `IUpdateable`, `UniformProvider`, `InspectProvider`, and `Renderer` are focused contracts. `Scene` remains a broad coordinator that clients may depend on for unrelated responsibilities.

### Dependency Inversion

The render pipeline depends on `Renderer`, `IRenderExtractor`, and provider abstractions. Graphics-API code is confined to the render layer: scene code writes uniforms through `IUniformSink` and allocates textures with the GL-neutral `TextureFormat`. Model and volume loading still depend directly on filesystem paths, Assimp, GLM, and concrete file formats. The engine boundary itself is improved by keeping Qt out of the DLL.

## Source Map

- Target and dependency ownership: `CMakeLists.txt`
- Public facade and host contracts: `engine/include/public/Engine.h`, `engine/include/public/graphics/{IGraphicsContextProvider,ScreenFrame}.h`
- Runtime (engine thread, locks, screens): `engine/include/runtime/EngineRuntime.h`, `engine/src/runtime/EngineRuntime.cpp`
- Input queue: `engine/include/runtime/input/InputEventQueue.h`
- Locking service decorators: `engine/include/runtime/inspection/LockedInspectionService.h`, `engine/include/runtime/settings/LockedSettingsService.h`, `engine/include/runtime/dashboard/LockedSceneDashboardService.h`
- Threading primitives: `engine/include/foundation/threading/{BackgroundTask,Cancellation,SharedProgress}.h`
- Preprocessing jobs and shader cache: `core/include/DtiVolumeScene.h`, `core/src/DtiVolumeScene.cpp`
- DTI volume texture packing: `engine/include/scene/volume/DTIVolume.h` (`PackDTIVolumeTextures`)
- Scene orchestration: `engine/include/scene/Scene.h`, `engine/src/scene/Scene.cpp`
- Slice plane: `engine/include/scene/slicing/SlicePlane.h`, `core/include/SliceController.h`, `engine/include/scene/geometry/{LineLoopGeometry,ConvexPolygonFillGeometry}.h`, `engine/shaders/unlit/`
- SH glyphs: `engine/include/scene/glyphs/{SHGlyphField,SHVolume}.h`, `engine/include/scene/spherical_harmonics/RealSH.h`, `engine/include/scene/geometry/IcosphereGeometry.h`, `engine/shaders/sh_glyph/`, and the DTI producer `core/src/preprocessing/stages/dti/DtiTensorShStage.cpp`
- Neutral inspection: `engine/include/public/inspection/InspectField.h`
- Scene dashboard (scene-wide fields): `engine/include/public/dashboard/ISceneDashboardService.h`, `engine/include/runtime/dashboard/SceneDashboardService.h`
- Frame construction: `engine/include/scene/extraction/RenderFrameBuilder.h`
- Extraction registry: `engine/include/scene/extraction/ExtractionRegistry.h`
- Render proxy: `engine/include/scene/extraction/RenderProxy.h`. The typed render commands it carries are render-layer types in `engine/include/render/core/RenderCommands.h`.
- Renderer, passes, GPU resources, and graphics-API initialization: the render layer under `engine/include/render/` + `engine/src/render/` (see [render layer](render.md))

## Multipass Rendering

Multipass rendering is owned by the render layer. `ForwardRenderer` registers its passes (`MeshGeometryPass`, `InstancedMeshGeometryPass`, `VolumePass`, `SkyboxPass`, `TransparentPass`) in a vector, and they execute in insertion order. The postprocessing chain (`AntiAliasingPass`) runs after them. See [render layer](render.md) for where new passes live and how they reach resources.

From the engine side, a new feature (for example particles) defines its typed command in the render layer's `RenderCommands.h`. Its scene objects append that command to a `RenderDataStore` through `RenderProxy`, and a matching pass consumes the channel. The existing passes do not need to know about the new channel.
