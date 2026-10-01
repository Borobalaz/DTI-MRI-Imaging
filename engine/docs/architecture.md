# Engine Architecture

## Purpose

The engine is the `engine` CMake shared-library target. It owns scene state, OpenGL rendering, resources, input state, volume data, and neutral inspection contracts. The target is built from `engine/src`, exports headers from `engine/include/engine`, and does not depend on Qt.

Primary build evidence: `CMakeLists.txt` (`add_library(engine SHARED ...)`). Representative runtime entry points are `engine/include/engine/Scene/Scene.h`, `engine/include/engine/Renderer/ForwardRenderer.h`, and `engine/include/engine/RenderCore/RenderFrameBuilder.h`.

## Top-Down Decomposition

| Module | Role | Main inputs | Main outputs | Ownership |
|---|---|---|---|---|
| Scene | Coordinates camera, lights, drawables, updateables, shaders, input, and inspection providers | Input state, scene objects, resources | Scene snapshots, render proxies, inspection providers | Engine owns the scene graph and contained shared objects |
| Camera and input | Maintains view/projection state and movement behavior | `InputState`, elapsed time, movement components | Camera matrices and updated transforms | Camera owns its movement component; UI only supplies input events |
| Geometry and drawable objects | Represents mesh geometry, game objects, volumes, and renderable content | Geometry, materials, transforms, volume data | `RenderProxy` values and updates | Scene owns drawable/updateable shared pointers |
| Resources | Loads and manages shaders, materials, textures, models, and volume files | File paths, OpenGL context, decoded data | GPU resources and reusable resource objects | Resource objects are shared through engine-owned `std::shared_ptr` values |
| Render core | Converts typed scene render commands into renderer-consumable frame data | `SceneSnapshot`, renderer descriptor, extractor registry | `RenderFrame` and `RenderDataStore` | Registry owns extractor instances and ordering metadata |
| Renderer | Executes a renderer strategy against a built frame | `RenderFrame` | OpenGL draw calls | `Renderer` provides the polymorphic contract; `ForwardRenderer` is the current implementation |
| Inspection contract | Describes editable engine state without Qt types | Provider state and callbacks | `InspectField` metadata, values, and setters | Engine owns providers and neutral field objects |

## Runtime Flow

1. The UI creates an OpenGL context and calls `InitializeEngineOpenGL` from `engine/include/engine/EngineOpenGL.h`.
2. The UI creates `Scene`; the scene constructs its camera and default resources/content in `engine/src/Scene/Scene.cpp`.
3. The scene receives input and updates cameras, objects, lights, and volumes.
4. `Scene::CreateSnapshot` gathers typed mesh, volume, and skybox render commands into a `SceneSnapshot`.
5. `RenderFrameBuilder::Build` transfers those commands into the frame data store and invokes additional enabled extractors.
6. A renderer such as `ForwardRenderer` visits the typed commands and owns shader, resource, and OpenGL execution.
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
- Render submission now uses independently typed `RenderDataStore` channels and a registered `IRenderPass` extension point. The built-in forward renderer still contains the current mesh, volume, and skybox consumers; future work can move those consumers into built-in pass classes without changing the submission contract.

### Open/Closed

The extractor registry and `Renderer` abstraction provide extension points for new extraction behavior and renderer implementations. Adding default scene content or new resource behavior still requires changes in concrete scene/resource code.

### Liskov Substitution

`Renderer` and `InspectProvider` define substitution points used by the UI and render flow. The code relies on providers returning valid names/fields and renderers honoring their descriptors; those behavioral expectations are implicit rather than formally documented in code contracts.

### Interface Segregation

`IDrawable`, `IUpdateable`, `UniformProvider`, `InspectProvider`, and `Renderer` are focused contracts. `Scene` remains a broad coordinator that clients may depend on for unrelated responsibilities.

### Dependency Inversion

The render pipeline depends on `Renderer`, `IRenderExtractor`, and provider abstractions. Resource loading and shader/model/volume implementations still depend directly on OpenGL, filesystem paths, Assimp, GLM, and concrete file formats. The engine boundary itself is improved by keeping Qt out of the DLL.

## Source Map

- Target and dependency ownership: `CMakeLists.txt`
- Scene orchestration: `engine/include/engine/Scene/Scene.h`, `engine/src/Scene/Scene.cpp`
- Neutral inspection: `engine/include/engine/Inspection/InspectField.h`
- Frame construction: `engine/include/engine/RenderCore/RenderFrameBuilder.h`
- Extraction registry: `engine/include/engine/RenderCore/ExtractionRegistry.h`
- Typed render commands: `engine/include/engine/Renderer/RenderProxy.h`
- Extensible render-pass contract: `engine/include/engine/Renderer/RenderPass.h`; concrete built-in passes live under `engine/include/engine/Renderer/Passes` and `engine/src/Renderer/Passes`. `Renderer` executes its pass vector in registration order.

## Multipass Rendering

`ForwardRenderer` is now an orchestration layer. It registers passes in a vector, and they execute in that insertion order rather than through a separate registry or automatic sorting:

1. `MeshGeometryPass` consumes `MeshRenderBatch` and performs forward geometry/material submission. Lighting is currently evaluated by the bound material shaders during this pass; there is no separate light-buffer pass in the forward renderer.
2. `VolumePass` consumes `VolumeRenderCommand` and owns volume blending/depth state.
3. `SkyboxPass` consumes `SkyboxRenderCommand` and owns skybox depth/culling state.
4. Additional passes can be registered through `Renderer::AddRenderPass`. They consume their own typed `RenderDataStore` channel without changing the central render command model.

A particle feature can therefore define `ParticleDraw`, submit it into `RenderDataStore`, and register a `ParticlePass` with its own phase and capability requirements. The existing built-in passes do not need to know about the particle channel.
- Renderer contract and implementation: `engine/include/engine/Renderer/Renderer.h`, `engine/include/engine/Renderer/ForwardRenderer.h`
- OpenGL initialization boundary: `engine/include/engine/EngineOpenGL.h`, `engine/src/EngineOpenGL.cpp`
