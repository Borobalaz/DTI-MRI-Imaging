# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

C++17 Qt 6 Widgets + OpenGL application for DTI/MRI visualization (diffusion tensor imaging, tractography, volume rendering). Built with CMake, dependencies via vcpkg.

Two CMake targets, one process:

- `engine` — Qt-independent `SHARED` library: rendering, scene graph, geometry, textures, volumes, input, inspection contracts.
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

There is no automated test suite in this repo (no test target, no test framework dependency).

A release-build workflow (triggered on push to `main` with `[release]` in the head commit message; builds Release on `windows-latest` and publishes exe + DLLs to the `release` branch under `release/windows`) lives at `.agents/workflows/build-release-branch.yml`. **GitHub Actions only picks up workflows under `.github/workflows/`**, so as long as the file stays under `.agents/` instead, this CI job will not run — confirm whether that's intentional before relying on it, or whether it needs to be restored under `.github/workflows/`.

## Architecture

### Engine/UI boundary is deliberately narrow

`engine`'s public headers (`CMakeLists.txt`: `target_include_directories(engine PUBLIC engine/include)`) are just `engine/include/Engine.h` and `engine/include/Inspection/*.h`. Everything under `engine/src/include/**` (Scene, Renderer, RenderCore, Camera, Volume, Material, Texture, Light, Geometry, Uniform, Input, Postprocessing) is `PRIVATE` to the `engine` target and **not visible to `app_qt`**.

`Engine.h` is explicit about this: *"Public façade for the engine library. This is the only class UI code should depend on; all other engine headers are private implementation details."* In practice `ui/include/widgets/OpenGLViewportWidget.h` includes only `Engine.h` (plus Qt headers) — confirm this when adding new engine functionality: expose it through `Engine`'s facade methods/structs rather than reaching into engine-private headers from UI code.

Crossing the boundary happens through:
- `Engine` facade methods: lifecycle (`InitializeOpenGL`, `CreateScene`), per-frame (`Update`, `Render`, `SetViewportSize`), input (`OnKeyChanged`, `OnMouseButtonChanged`, `OnMousePositionChanged`, `OnScroll`), picking (`ScreenPointToRay`).
- The **inspection contract** (`IInspectionService`, `InspectField`, `InspectHandle`): a Qt-free way for the engine to describe editable scene state. `ui/src/qt-adapters/QTSceneInspector.cpp` adapts these neutral fields into Qt widgets (`ui/include/widgets/inspect_fields/`) and routes edits back through field setters/callbacks.

> Note: `docs/*.md` / `docs/*.mmd` at repo root (sequence/class diagrams) describe an earlier ImGui+GLFW main-loop prototype (`GuiRoot`, `PanelRegistry`, `RuntimeControlsPanel`) and, in places, a wider engine public surface (`Scene.h`, `ForwardRenderer.h` as public headers) that no longer matches the CMake include visibility or `Engine.h`'s own doc comment. Treat `engine/docs/architecture.md` and `ui/docs/architecture.md` as the current, maintained source of truth for each subsystem; treat root `docs/` as historical/possibly stale and verify against code before relying on it.

### Engine internals (private, under `engine/src/include` + `engine/src`)

- **Scene** coordinates camera, lights, drawables/updateables, shader registry, input, and inspection providers; it also constructs default scene content. It builds a `SceneSnapshot` of typed render commands per frame.
- **Render pipeline**: `RenderFrameBuilder` turns a `SceneSnapshot` into a `RenderFrame`/`RenderDataStore` via a registry of `IRenderExtractor`s. `ForwardRenderer` (the current `Renderer` implementation) runs a vector of `IRenderPass`es in registration order — currently `MeshGeometryPass`, `VolumePass`, `SkyboxPass` (`engine/src/include/Renderer/Passes`, `engine/src/Renderer/Passes`). New render features (e.g. particles) add a typed render command + a new pass registered via `Renderer::AddRenderPass`, without touching existing passes.
- **Volume hierarchy**: abstract `Volume` (owns transform, `VolumeGeometry`, `Shader`, `VolumeTextureSet`; implements `UniformProvider`, `IDrawable`, `InspectProvider`) has two concrete subclasses: `FloatVolume` (single scalar channel) and `DTIVolume` (17 `VolumeData` channels — 6 tensor components Dxx/Dyy/Dzz/Dxy/Dxz/Dyz, 3 eigenvector components, 3 eigenvalues L1-L3, 4 scalars FA/MD/AD/RD, 1 mask — packed into 5 RGB/RGBA 3D textures, with selectable render modes and a Z-slice parameter). See `docs/VOLUME_INHERITANCE_DIAGRAM.md`.
- Everything renderable/updatable implements `UniformProvider` (binds uniforms to a `Shader`) and, where applicable, `IDrawable`/`IUpdateable`/`InspectProvider`.

### Staged MRI preprocessing pipeline (`core/`)

`core` is a generic C++ staged MRI preprocessing pipeline (not Qt- or engine-dependent) with pluggable, model-specific reconstruction stages. Pattern: `MriPreprocessingPipeline` holds an ordered `vector<unique_ptr<IMriPreprocessingStage>>`; each stage implements `Name()`/`Execute(MriPreprocessingContext&)` and mutates a shared context. DTI (diffusion tensor imaging) is the first concrete reconstruction model implemented: `MriToDtiPreprocessor` assembles the DTI stage list and turns raw DWI/bval/bvec input into `DTIVolumeChannels` (exposed on the pipeline's result/context as `dtiChannels`/`outputDtiChannels`) plus surface/streamline meshes; `MriPreprocessingRunner` wraps construction + execution + writing results to disk. fODF (fiber orientation distribution function) reconstruction is planned as a second, parallel model.

Stages that are generic (not tied to any reconstruction model) stay flat under `core/include/Preprocessing/stages/` / `core/src/preprocessing/stages/`: input validation, gradient normalization (bval/bvec), brain surface mesh (isosurface), and the [0,1] channel normalization pass. Stages specific to a reconstruction model live in a model-named subfolder — DTI's under `core/include/Preprocessing/stages/dti/` / `core/src/preprocessing/stages/dti/` (`DtiTensorSynthesisStage`, `DtiPrincipalEigenvectorStage`, `DtiScalarSynthesisStage`, `DtiFiberTractographyStage`); a future fODF model would get its own `stages/fodf/` sibling. Factory declarations follow the same split: `core/include/Preprocessing/stages/CommonPreprocessingStages.h` for generic factories, `core/include/Preprocessing/stages/dti/DtiPreprocessingStages.h` for DTI's. Concrete preprocessor assemblers live under `core/include/Preprocessing/preprocessors/` / `core/src/preprocessing/preprocessors/` (currently `MriToDtiPreprocessor`; a future `MriToFodfPreprocessor` would be a sibling).

Stage order (see `docs/DTI_PREPROCESSOR_CLASSDIAGRAM.md` for full sequence/field detail — note: stale/historical, not updated for this rename): input validation → gradient normalization (bval/bvec) → tensor synthesis (least-squares fit of the 3×3 diffusion tensor) → principal eigenvector/eigenvalue extraction → scalar synthesis (FA/MD/AD/RD) → brain surface mesh (isosurface) → fiber tractography (seed at FA > threshold, trace along principal eigenvector, stop at FA/eigenvalue thresholds, build tube mesh) → normalization (scale channels to [0,1]).

Tractography parameters live in `MriTractographySettings` (FA seed/stop thresholds, step size in voxels, max steps, seed stride/max seeds, tube radius/segments) and it implements `InspectProvider` so its fields are editable through the same inspection mechanism as scene objects.

To add a stage: implement `IMriPreprocessingStage`, add a factory function (in the generic or model-specific factory header as appropriate), and register it via `AddStage()` in the relevant preprocessor's constructor (e.g. `MriToDtiPreprocessor`).

Saved preprocessing datasets (payload files + a `metadata.json` provenance/parameters manifest) are read/written through `core/include/Preprocessing/io/` — a registry (`PreprocessingDatasetFormatRegistry`) dispatches `IPreprocessingDatasetWriter`/`IPreprocessingDatasetReader` implementations by a `preprocessorType` string, so each reconstruction model's save/load format is independent. DTI's implementation lives under `io/dti/` (`DtiPreprocessingDatasetWriter`/`Reader`, plus `DtiPreprocessingParameters` for provenance JSON); to add a new preprocessor's saved-dataset support (e.g. a future fODF model), implement the two interfaces for it and register both inside `GetDefaultPreprocessingDatasetFormatRegistry()` (`core/src/preprocessing/io/PreprocessingDatasetFormats.cpp`) — no other file under `Preprocessing/io` needs to change. `DtiVolumeScene` exposes "Save current dataset..."/"Load dataset" actions through the same inspection contract as its other controls.

`DtiVolumeScene` (`core/include/DtiVolumeScene.h`) is the concrete DTI scene builder; it depends on engine-private `Scene`/`GameObject` types, a pre-existing exception to `core`'s engine-independence.

Separately, `scripts/run_hd_bet.py` is a standalone Python helper (HD-BET skull stripping) — not part of the C++ build/pipeline.

### UI layer (`ui/`)

`ui/src/app/main.cpp` sets up a 3.3 core-profile OpenGL surface and `QApplication`, then shows `WidgetsMainWindow` (`ui/src/windows`), which lays out `OpenGLViewportWidget` (viewport), `InspectorWidget`/`InspectProviderWidget` (editable fields), `SceneObjectListWidget`, and `RenderStatisticsWidget`. `OpenGLViewportWidget` (`QOpenGLWidget` subclass) owns the `Engine` instance, drives `Update`/`Render` from a `QTimer` inside `paintGL`, translates Qt input events into `Engine` input calls, and feeds `QTSceneInspector`. `DTIViewportWidget` specializes the viewport for DWI/bval/bvec dataset loading. Theming goes through `IThemeStyle`/`DarkThemeStyle`/`LightThemeStyle`.

### Documentation conventions

Subsystem docs are deliberately split and owned separately: engine architecture/diagrams live under `engine/docs/*.md` / `*.mmd`, UI under `ui/docs/*.md` / `*.mmd`. Cross-boundary behavior is documented on the owning side and linked from the other side — there is no combined root architecture doc (root `docs/` predates this convention and is not kept in sync; see note above). When updating these docs, keep code as ground truth and update docs to match code, not the reverse.

### `.agents/`

`.agents/agents/*.agent.md` are persona/instruction files for a different (VSCode/Copilot-style) agent runner, not Claude Code subagents (currently: `documentation`, `dti-mri-processing-planner`, `expert-cpp-software-engineer`, `glsl-gpu-implementer`, `qt-gui`) — but they encode house conventions worth following here too: keep changes scoped to what was asked (no unrelated refactors), prefer incremental edits over new abstractions, and keep engine/UI documentation changes confined to `engine/docs`/`ui/docs` as described above.

`.agents/skills/build-and-run/SKILL.md` reinforces using `build.ps1`/`build-and-run.ps1`/`debug.ps1`/`clean.ps1` rather than raw CMake or manually launching the exe, and to report a script's failure/output rather than silently falling back to a manual build.
