# Render Layer Architecture

## Purpose

The render layer is the `engine_render` CMake STATIC target, built from `engine/render/src`. It is the only code in the repository that includes `<glad/glad.h>` or calls the graphics API. It owns the GPU resource classes, the uniform contract, the renderer, and the render/postprocessing passes. Scene-side code in `engine/src` and `core/` uses the layer only through the public, GL-free headers under `engine/render/include/Render/`.

Build evidence: `CMakeLists.txt` (`add_library(engine_render STATIC ...)`). `glad::glad` and `OpenGL::GL` are linked `PRIVATE` to `engine_render`, and `engine_render` is linked `PRIVATE` into the `engine` DLL.

## Dependency direction

```
app_qt  -->  engine (SHARED)  -->  engine_render (STATIC)  -->  engine_foundation (STATIC)
```

- `engine_foundation` (`engine/foundation/`) holds the dependency-free utilities that both the layer and the engine need: `EngineLog` (`Log/EngineLog.h`), `StableTypeId` (`RenderCore/StableTypeId.h`), and the header-only `Threading/` primitives (`BackgroundTask`, `Cancellation`, `SharedProgress`).
- `engine_render` depends on `engine_foundation` and `glm`, and has `glad`/OpenGL as private dependencies. It never includes headers from `engine/src/include` or `core/`.
- `engine` (scene, volumes, concrete geometries, extraction, the `Engine` facade) depends on the layer's public headers. It never includes `engine/render/src`.

## Public vs private split

| Location | Visibility | Contents |
|---|---|---|
| `engine/render/include/Render/` | PUBLIC include dir of `engine_render` (included as `"Render/..."`) | GL-free API: `GraphicsApi.h`, `RenderTarget.h`, `TextureFormat.h`, `Shader.h`, `Material.h`, `PBRTextureSlots.h`, `VolumeTextureSet.h`, `Texture/{Texture,Texture2D,Texture3D,TextureCube,TextureData3D}.h`, `Geometry/{Geometry,InstanceBuffer}.h`, `Uniform/{IUniformSink,UniformProvider,CompositeUniformProvider,TypedUniformProvider}.h`, `Core/{RenderCommands,RenderDataStore,RenderFrame,RendererDescriptor}.h`, `Renderer/{Renderer,ForwardRenderer,RenderPass}.h`, `Postprocessing/IPostProcessingPass.h` |
| `engine/render/src/` | PRIVATE include dir of `engine_render` (included relative to `src`, e.g. `"RenderBackend.h"`, `"OpenGL/Framebuffer.h"`, `"Passes/VolumePass.h"`) | `RenderBackend`, plus the implementations of `Material`, `VolumeTextureSet`, `TextureData3D` and the `Uniform/` providers |
| `engine/render/src/OpenGL/` | private | The OpenGL implementations of the resources: `GraphicsApi.cpp` (glad loader, garbage collection, shutdown), `GpuDeletionQueue.h/.cpp`, `Shader.cpp`, `Texture*.cpp` with `TextureGL.h` (the `TextureFormat` to GL mapping and the stb channel-format helper), `Geometry.cpp`, `InstanceBuffer.cpp`, `Framebuffer.h/.cpp`, `RenderTarget.cpp`, `FullscreenTriangleGeometry.h/.cpp` |
| `engine/render/src/Renderer/` | private | `Renderer.cpp` (the postprocessing chain orchestration) and `ForwardRenderer.cpp` |
| `engine/render/src/Passes/` | private | `MeshGeometryPass`, `InstancedMeshGeometryPass`, `VolumePass`, `SkyboxPass` |
| `engine/render/src/Postprocessing/` | private | `AntiAliasingPass` (FXAA) and `PostProcessingShader` |

Because `engine/render/src` is not on `engine`'s include path, an `engine`/`core` file that includes `RenderBackend.h`, a pass header, or `Framebuffer.h` fails to compile. `<glad/glad.h>` is not reliably blocked this way: under vcpkg its header sits in the same shared `include/` directory as `glm`/`assimp`, and `glad`'s link dependency propagates from the static `engine_render` into `engine`. Keeping glad inside the layer is therefore a convention, checked by grep, not something the compiler enforces.

## The `RenderBackend` friend gateway

GPU-only members of the resource classes are `private`, and each resource class declares `friend class RenderBackend;`. `RenderBackend` (`engine/render/src/RenderBackend.h`) is a static-only, non-instantiable class, and it is the **only** friend. Because its header is private to the layer, only layer code can call it:

```cpp
static void          Use(Shader&);
static IUniformSink& Uniforms(Shader&);           // Shader inherits IUniformSink privately
static void          Bind(const Texture&, unsigned int unit);
static unsigned int  Handle(const Texture&);      // e.g. framebuffer attachments
static unsigned int  Handle(const InstanceBuffer&);
static void          Draw(const Geometry&, Shader&);
static void          DrawInstanced(const Geometry&, Shader&, const InstanceBuffer&);
static void          BindTarget(const RenderTarget&); // draw framebuffer + matching viewport
```

| Class | Public (CPU) API | Private, reachable only through `RenderBackend` |
|---|---|---|
| `Shader` | constructor, `GetId`, `IsValid`, `ReloadIfChanged`, `operator[]` (stored uniforms), `GetStoredUniforms`, `Apply(IUniformSink&)` | `programId`, `Use`, the `IUniformSink` setters, uniform reflection (`GetUniformInfo(s)`, `HasUniform`) |
| `Texture`, `Texture2D`, `Texture3D`, `TextureCube` | constructors, `IsValid` | `Bind`, `GetId` (private virtuals) |
| `Geometry` | `Generate`, `MarkGpuDirty`, `ComputeNormals`, `GetVertices`/`GetIndices`; protected `vertices`/`indices` and draw-mode setters | `Draw`, `DrawInstanced`, `VAO`/`VBO`/`EBO` |
| `InstanceBuffer` | `Upload`, `Reset`, `IsValid`, `GetInstanceCount` | `GetHandle` |
| `RenderTarget` | constructor (GL-free), `Resize`, `GetWidth`/`GetHeight`, `IsValid`, `ReadPixelsRGBA8` | `Bind` |

The resource implementations also go through the gateway when they use another resource's GPU members. For example, `Geometry::Draw` calls `RenderBackend::Use(shader)`, and `Shader::SetTexture` calls `RenderBackend::Bind`.

**Rules for new render code:**
- New render passes live in `engine/render/src/Passes/`, implement `IRenderPass`, and are registered in `ForwardRenderer`'s constructor through `Renderer::AddRenderPass`. They run in registration order.
- New postprocessing passes live in `engine/render/src/Postprocessing/`, implement `IPostProcessingPass`, and are registered through `Renderer::AddPostProcessingPass`. They never bind a framebuffer or set the viewport; `Renderer::ExecutePostProcessingPasses` does that.
- Passes access resources only through `RenderBackend`. A pass activates the shader with `RenderBackend::Use`, gets the sink with `RenderBackend::Uniforms`, sends every uniform/provider/texture write to that sink, and draws with `RenderBackend::Draw`/`DrawInstanced`. Direct GL state save/restore inside a pass is allowed, because it is layer code.
- Do not add a second friend or widen a private member to make scene code compile. Route scene code through `IUniformSink` or the resource's public CPU API instead.

## Uniform contract: `IUniformSink`

`UniformProvider::Apply(IUniformSink&)` is how scene objects (`Camera`, lights, `GameObject`, `Volume`/`DTIVolume`, `Scene`), `Material`, and the `TypedUniformProvider`/`CompositeUniformProvider` helpers publish uniforms. The sink has `SetBool`/`SetInt`/`SetFloat`/`SetVec2`/`SetVec3`/`SetMat4(name, value)` and `SetTexture(name, const Texture*, unsigned unit)`. `SetTexture` binds the texture when it is non-null and always sets the sampler uniform.

`Shader` is the concrete sink, but only `RenderBackend::Uniforms` hands it out. `Shader::Use()` activates the program and re-applies the shader's own stored (inspector-editable) uniforms via `Apply(*this)`.

Fixed texture units: material slots 0-4 (`PBR::TextureSlot` order), skybox cubemap 5, volume textures `8 + index` (`VolumeTextureSet`), FXAA scene color 0.

## `TextureFormat`

`Texture2D`'s allocation constructor and both `Texture3D` constructors (directly, or through `TextureData3D::format`) take a GL-neutral `TextureFormat` (`R32F`, `RGB32F`, `RGBA32F`, `RGBA8`, `Depth24`). `ToGLTextureFormat` in `OpenGL/TextureGL.h` maps each value to its GL (internal format, format, type) triple, so scene code such as `DTIVolume`/`FloatVolume` never names GL enums.

## Boundary data

- The scene side builds typed render commands (`Render/Core/RenderCommands.h`: `MeshRenderCommand`/`MeshRenderBatch`, `MeshInstancedRenderCommand`/`MeshInstancedRenderBatch`, `VolumeRenderCommand`, `SkyboxRenderCommand`). It appends them to a `RenderDataStore` through the engine-side `RenderProxy` (`engine/src/include/Renderer/RenderProxy.h`), and the engine's `RenderFrameBuilder` assembles a `RenderFrame`.
- The layer consumes that frame in `Renderer::Draw`.
- `MeshInstancedRenderCommand::instances` is a non-owning `const InstanceBuffer*` owned by the submitting `GameObject`.
- Postprocessing is gated per frame through `ForwardRenderer::SetPostProcessingEnabled`. The engine runtime sets it each render from the `postprocessing.enabled` engine setting.
- `Renderer::Draw(frame)` renders into whatever framebuffer is bound on entry. `Renderer::Draw(frame, const RenderTarget&)` (`ForwardRenderer`) resizes the renderer to the target, binds the target through `RenderBackend::BindTarget`, and runs the same `Draw`. Postprocessing already composites into the framebuffer bound on entry, so both paths behave the same.

## Threading: the GL thread is wherever the context is current

The layer has no thread of its own and no knowledge of one. "The GL thread" is whichever thread has the context current and calls into the layer: the engine thread in the `Engine`'s threaded mode, or the host's thread in sync mode. Every graphics-API call, every first draw/bind (which triggers lazy uploads), and `CollectGraphicsGarbage`/`ShutdownGraphicsApi` must run there. Construction and destruction of resources are deliberately safe off that thread where noted below.

### Lazy `Geometry` upload

`Geometry` construction is GL-free, so meshes can be built on a preprocessing worker. `Generate()` runs in each concrete ctor, so tube expansion and similar work stay on the CPU thread that builds it. GPU buffers are created on the first `Draw`/`DrawInstanced` through the private `EnsureGpuResident()`, which also re-buffers after `MarkGpuDirty()`. The upload only *reads* `vertices`/`indices`. Once a geometry has been handed to the GL thread, nothing may mutate them, because that would race with the upload and with off-thread readers such as dataset saving.

### Lazy `Texture3D` upload and `TextureData3D`

`TextureData3D` (`Render/Texture/TextureData3D.h`) is a GL-free CPU payload: `dimensions`, a `format` (`R32F`, `RGB32F` or `RGBA32F` only) and interleaved float `texels` (x fastest, then y, then z). `IsWellFormed()` checks that the dimensions are positive, the format is supported, and the size equals `w*h*d*components`.

`Texture3D` has two constructors:

- **Eager** `(width, height, depth, format, data, linear)`: uploads immediately, so it must run on the GL thread. `FloatVolume` uses it.
- **Lazy** `(TextureData3D, linear)`: GL-free, so it can run on any thread. A malformed payload logs an error and marks the texture failed. Otherwise the texels are kept as pending. `DTIVolume` uses this path; its payloads are packed on the preprocessing worker by `PackDTIVolumeTextures`.

At the first `Bind(unit)` of a pending texture:

1. The texture activates the unit and drains stale `glGetError` state. Draining is required because the upload now happens mid-frame.
2. It checks the size against `GL_MAX_3D_TEXTURE_SIZE`, then creates and uploads the texture, leaving it bound on that unit.
3. It becomes resident, or failed with an error naming the dimensions and format.
4. It frees the CPU texels either way.

A failed or not-yet-resident texture binds 0 on its unit. `IsValid()` is `true` while a texture is pending, so an upload failure surfaces one frame after construction. A texture that is never bound, such as one on a hidden volume, keeps its CPU texels until it is.

### `GpuDeletionQueue`

Destructors of every GPU-owning resource (`Texture2D/3D/Cube`, `Shader`, `Geometry`, `InstanceBuffer`, `Framebuffer`) may run on any thread. For example, the UI thread can drop the last `shared_ptr` to an inspectable provider. So these destructors never call the graphics API. They call `EnqueueGpuDeletion(kind, handle)` (`OpenGL/GpuDeletionQueue.h`, private, mutex-guarded), and the GL thread deletes the queued objects in `FlushGpuDeletions()`.

The queue is process-global. That assumes one GL context per process, the same assumption glad's global function table makes. It accepts handles only between `InitializeGraphicsApi` and `ShutdownGraphicsApi`, so destructors that run after the context is gone don't grow it.

### `RenderTarget`

`RenderTarget` (`Render/RenderTarget.h`, `OpenGL/RenderTarget.cpp`) is an offscreen color + depth target wrapping the private `Framebuffer`. Its constructor is GL-free; everything else runs on the GL thread.

- `Resize(w, h)` reallocates only when the size changes, and ignores a non-positive size.
- `ReadPixelsRGBA8(vector<uint8_t>&)` reads the color attachment with pack alignment 4 and reverses the rows, so the result is tightly packed RGBA8 **top-down**.
- Binding is GPU-only, through `RenderBackend::BindTarget`. That is the only friend path.

The engine runtime owns one `RenderTarget` per screen.

## Initialization and shutdown

`InitializeGraphicsApi(GraphicsProcLoader)` (`Render/GraphicsApi.h`, implemented in `OpenGL/GraphicsApi.cpp` with `gladLoadGLLoader`) loads the entry points for the current context and starts accepting GPU deletions. The engine runtime calls it from `Engine::InitializeOpenGL` (sync mode) or on the engine thread during `Engine::Start` (threaded mode). The runtime then creates the `ForwardRenderer`, which owns GPU resources, and calls `Renderer::InitializeGLState`.

- **`CollectGraphicsGarbage()`**: GL thread. It flushes the deletion queue. The runtime calls it once per render, before drawing.
- **`ShutdownGraphicsApi()`**: GL thread, after every render resource the caller owns has been destroyed. It does a final flush and then stops accepting deletions. The runtime's teardown order is scene -> screens -> renderer -> `CollectGraphicsGarbage` -> `ShutdownGraphicsApi`.
