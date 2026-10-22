# Engine Architecture

## Purpose

The engine is the `engine` CMake shared-library target. It owns scene state, input state, volume data, concrete geometries, scene-to-frame extraction, and neutral inspection contracts. The target is built from `engine/src` (plus `core/src`), exports headers from `engine/include`, and does not depend on Qt.

GPU resources (shaders, materials, textures, geometry buffers), the renderer, its passes, and every graphics-API call live in the separate **render layer**: the `engine_render` static target under `engine/render/`. `engine` links it `PRIVATE`. Both link the `engine_foundation` static target (`engine/foundation/`: `EngineLog`, `StableTypeId`), so the dependency direction is `engine` -> `engine_render` -> `engine_foundation`. See [the render layer architecture](../render/docs/architecture.md) for the layer's public/private split and its `RenderBackend` gateway rule.

Primary build evidence: `CMakeLists.txt` (`add_library(engine SHARED ...)`, `add_library(engine_render STATIC ...)`, `add_library(engine_foundation STATIC ...)`). Representative runtime entry points are `engine/src/include/Scene/Scene.h`, `engine/src/include/RenderCore/RenderFrameBuilder.h`, and the layer's `engine/render/include/Render/Renderer/ForwardRenderer.h`.

## Top-Down Decomposition

| Module | Role | Main inputs | Main outputs | Ownership |
|---|---|---|---|---|
| Scene | Coordinates camera, lights, drawables, updateables, shaders, input, and inspection providers | Input state, scene objects, resources | Scene snapshots, render proxies, inspection providers | Engine owns the scene graph and contained shared objects |
| Camera and input | Maintains view/projection state and movement behavior | `InputState`, elapsed time, movement components | Camera matrices and updated transforms | Camera owns its movement component; UI only supplies input events |
| Geometry and drawable objects | Represents mesh geometry, game objects, volumes, and renderable content | Geometry, materials, transforms, volume data | `RenderProxy` values and updates | Scene owns drawable/updateable shared pointers |
| Resources | Loads models and volume files, and creates render-layer resources (shaders, materials, textures) for scene content | File paths, decoded data | Resource objects; their GPU-side behavior lives in the render layer | Resource objects are shared through engine-owned `std::shared_ptr` values |
| Render core | Converts typed scene render commands into renderer-consumable frame data | `SceneSnapshot`, renderer descriptor, extractor registry | `RenderFrame` and `RenderDataStore` (render-layer types) | Registry owns extractor instances and ordering metadata |
| Renderer (render layer) | Executes a renderer strategy against a built frame | `RenderFrame` | Graphics-API draw calls | Lives in `engine_render`; the `Engine` facade owns the `ForwardRenderer` instance. See [render layer](../render/docs/architecture.md) |
| Inspection contract | Describes editable engine state without Qt types | Provider state and callbacks | `InspectField` metadata, values, and setters | Engine owns providers and neutral field objects |

## Runtime Flow

1. The UI creates an OpenGL context and calls `Engine::InitializeOpenGL` (`engine/include/Engine.h`), which forwards to the render layer's `InitializeGraphicsApi` (`engine/render/include/Render/GraphicsApi.h`).
2. The UI creates `Scene`; the scene constructs its camera and default resources/content in `engine/src/Scene/Scene.cpp`.
3. The scene receives input and updates cameras, objects, lights, and volumes.
4. `Scene::CreateSnapshot` gathers typed mesh, volume, and skybox render commands into a `SceneSnapshot`.
5. `RenderFrameBuilder::Build` transfers those commands into the frame data store and invokes additional enabled extractors.
6. The render layer's `ForwardRenderer` runs its registered passes over the typed commands. All shader, resource, and graphics-API execution happens there.
7. The scene exposes `InspectProvider` instances and neutral `InspectField` values to the UI adapter.

The engine resource files are stored under `engine/assets` and `engine/shaders`. CMake copies them beside the UI executable at build time; engine runtime paths therefore remain relative to the deployed application directory.

## Boundary Contract

The engine-to-UI boundary is intentionally narrow in dependency direction:

- The UI links to the `engine` DLL and includes engine public headers.
- The engine does not include Qt or UI widget headers.
- `InspectProvider`, `InspectField`, and `InspectValue` cross the boundary as neutral C++ contracts.
- The UI converts those contracts into Qt objects in `QTSceneInspector`.
- The UI owns event-loop, widget, signal, and adapter lifetimes.

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
- Scene orchestration: `engine/src/include/Scene/Scene.h`, `engine/src/Scene/Scene.cpp`
- Neutral inspection: `engine/include/Inspection/InspectField.h`
- Frame construction: `engine/src/include/RenderCore/RenderFrameBuilder.h`
- Extraction registry: `engine/src/include/RenderCore/ExtractionRegistry.h`
- Render proxy: `engine/src/include/Renderer/RenderProxy.h`. The typed render commands it carries are render-layer types in `engine/render/include/Render/Core/RenderCommands.h`.
- Renderer, passes, GPU resources, and graphics-API initialization: the render layer under `engine/render/` (see [render layer](../render/docs/architecture.md))

## Multipass Rendering

Multipass rendering is owned by the render layer. `ForwardRenderer` registers its passes (`MeshGeometryPass`, `InstancedMeshGeometryPass`, `VolumePass`, `SkyboxPass`) in a vector, and they execute in insertion order. The postprocessing chain (`AntiAliasingPass`) runs after them. See [render layer](../render/docs/architecture.md) for where new passes live and how they reach resources.

From the engine side, a new feature (for example particles) defines its typed command in the render layer's `RenderCommands.h`. Its scene objects append that command to a `RenderDataStore` through `RenderProxy`, and a matching pass consumes the channel. The existing passes do not need to know about the new channel.
