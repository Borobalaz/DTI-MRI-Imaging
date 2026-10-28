# Render Layer Architecture

## Purpose

The render layer is the `engine_render` CMake STATIC target, built from `engine/src/render/`. It is the only code in the repository that includes `<glad/glad.h>` or calls the graphics API. It owns the GPU resource classes, the uniform contract, the renderer, and the render/postprocessing passes. Scene-side code (`engine/src/scene`, `engine/src/runtime`) and `core/` uses the layer only through its GL-free headers under `engine/include/render/` (excluding `render/internal/`).

Build evidence: `CMakeLists.txt` (`add_library(engine_render STATIC ...)`). `glad::glad` and `OpenGL::GL` are linked `PRIVATE` to `engine_render`, and `engine_render` is linked `PRIVATE` into the `engine` DLL.

## Dependency direction

```
app_qt  -->  engine (SHARED)  -->  engine_render (STATIC)  -->  engine_foundation (STATIC)
```

- `engine_foundation` (`engine/include/foundation/` + `engine/src/foundation/`) holds the dependency-free utilities that both the layer and the engine need: `EngineLog` (`foundation/log/EngineLog.h`), `StableTypeId` (`foundation/types/StableTypeId.h`), and the header-only `foundation/threading/` primitives (`BackgroundTask`, `Cancellation`, `SharedProgress`).
- `engine_render` depends on `engine_foundation` and `glm`, and has `glad`/OpenGL as private dependencies. It never includes headers from `engine/include/{scene,runtime}` or `core/`.
- `engine` (scene, volumes, concrete geometries, extraction, the `Engine` facade) depends on the layer's public headers. It never includes `engine/include/render/internal/`.

## Public vs private split

| Location | Visibility | Contents |
|---|---|---|
| `engine/include/render/` | Layer API (included as `"render/..."`) | GL-free API: `GraphicsApi.h`, `RenderTarget.h`, `TextureFormat.h`, `Shader.h`, `Material.h`, `PBRTextureSlots.h`, `VolumeTextureSet.h`, `texture/{Texture,Texture2D,Texture3D,TextureCube,TextureData3D,BufferTexture}.h`, `geometry/{Geometry,InstanceBuffer}.h`, `uniform/{IUniformSink,UniformProvider,CompositeUniformProvider,TypedUniformProvider}.h`, `core/{RenderCommands,RenderDataStore,RenderFrame,RendererDescriptor}.h`, `renderer/{Renderer,ForwardRenderer,RenderPass}.h`, `postprocessing/IPostProcessingPass.h` |
| `engine/include/render/internal/` | Layer-internal (included as `"render/internal/..."`, only from render-layer files) | `RenderBackend.h`; `opengl/{Framebuffer,FullscreenTriangleGeometry,GpuDeletionQueue,TextureGL}.h`; `passes/{MeshGeometryPass,InstancedMeshGeometryPass,VolumePass,SkyboxPass}.h`; `postprocessing/{AntiAliasingPass,PostProcessingShader}.h` |
| `engine/src/render/` | private sources | `RenderBackend.cpp`, plus the implementations of `Material`, `VolumeTextureSet`, `TextureData3D` and the `uniform/` providers |
| `engine/src/render/opengl/` | private sources | The OpenGL implementations of the resources: `GraphicsApi.cpp` (glad loader, garbage collection, shutdown), `GpuDeletionQueue.cpp`, `Shader.cpp`, `Texture*.cpp` (with `internal/opengl/TextureGL.h`: the `TextureFormat` to GL mapping and the stb channel-format helper), `Geometry.cpp`, `InstanceBuffer.cpp`, `Framebuffer.cpp`, `RenderTarget.cpp`, `FullscreenTriangleGeometry.cpp` |
| `engine/src/render/renderer/` | private sources | `Renderer.cpp` (the postprocessing chain orchestration) and `ForwardRenderer.cpp` |
| `engine/src/render/passes/` | private sources | `MeshGeometryPass`, `InstancedMeshGeometryPass`, `VolumePass`, `SkyboxPass` |
| `engine/src/render/postprocessing/` | private sources | `AntiAliasingPass` (FXAA) and `PostProcessingShader` |

`engine/include` is on the include path of every engine target, so the compiler alone does not stop an `engine`/`core` file from including `render/internal/...`; internal headers may only be included from render-layer files, which `check-layering.ps1` (run by `build.ps1` before configuring) enforces. `<glad/glad.h>` is not blocked by the build either: under vcpkg its header sits in the same shared `include/` directory as `glm`/`assimp`, and `glad`'s link dependency propagates from the static `engine_render` into `engine`. Keeping glad inside `engine/src/render/` (and the `render/internal/opengl/` headers) is therefore enforced by `check-layering.ps1`, not by the compiler.

## The `RenderBackend` friend gateway

GPU-only members of the resource classes are `private`, and each resource class declares `friend class RenderBackend;`. `RenderBackend` (`engine/include/render/internal/RenderBackend.h`) is a static-only, non-instantiable class, and it is the **only** friend. Because its header is internal to the layer, only layer code can call it:

```cpp
static void          Use(Shader&);
static IUniformSink& Uniforms(Shader&);           // Shader inherits IUniformSink privately
static void          Bind(const Texture&, unsigned int unit);
static unsigned int  Handle(const Texture&);      // e.g. framebuffer attachments
static unsigned int  Handle(const InstanceBuffer&);   // transform VBO (locations 4-7)
static void          EnsureResident(const InstanceBuffer&);
static const std::vector<InstanceAttributeBinding>& AttributeBindings(const InstanceBuffer&);
static void          Draw(const Geometry&, Shader&);
static void          DrawInstanced(const Geometry&, Shader&, const InstanceBuffer&);
static void          BindTarget(const RenderTarget&); // draw framebuffer + matching viewport
```

| Class | Public (CPU) API | Private, reachable only through `RenderBackend` |
|---|---|---|
| `Shader` | constructor, `GetId`, `IsValid`, `ReloadIfChanged`, `operator[]` (stored uniforms), `GetStoredUniforms`, `Apply(IUniformSink&)` | `programId`, `Use`, the `IUniformSink` setters, uniform reflection (`GetUniformInfo(s)`, `HasUniform`) |
| `Texture`, `Texture2D`, `Texture3D`, `TextureCube`, `BufferTexture` | constructors, `IsValid` (`BufferTexture`: also `SetData`) | `Bind`, `GetId` (private virtuals) |
| `Geometry` | `Generate`, `MarkGpuDirty`, `ComputeNormals`, `GetVertices`/`GetIndices`; protected `vertices`/`indices` and draw-mode setters | `Draw`, `DrawInstanced`, `VAO`/`VBO`/`EBO` |
| `InstanceBuffer` | `SetTransforms`, `AddAttributeStream`, `AddBufferStream`, `SetStreamData`, `Clear`, `IsValid`, `GetInstanceCount`, `Apply(IUniformSink&)` | `EnsureGpuResident`, `GetTransformHandle`, `GetAttributeBindings` |
| `RenderTarget` | constructor (GL-free), `Resize`, `GetWidth`/`GetHeight`, `IsValid`, `ReadPixelsRGBA8` | `Bind` |

The resource implementations also go through the gateway when they use another resource's GPU members. For example, `Geometry::Draw` calls `RenderBackend::Use(shader)`, and `Shader::SetTexture` calls `RenderBackend::Bind`.

**Rules for new render code:**
- New render passes live in `engine/src/render/passes/` (headers in `engine/include/render/internal/passes/`), implement `IRenderPass`, and are registered in `ForwardRenderer`'s constructor through `Renderer::AddRenderPass`. They run in registration order.
- New postprocessing passes live in `engine/src/render/postprocessing/` (headers in `engine/include/render/internal/postprocessing/`), implement `IPostProcessingPass`, and are registered through `Renderer::AddPostProcessingPass`. They never bind a framebuffer or set the viewport; `Renderer::ExecutePostProcessingPasses` does that.
- Passes access resources only through `RenderBackend`. A pass activates the shader with `RenderBackend::Use`, gets the sink with `RenderBackend::Uniforms`, sends every uniform/provider/texture write to that sink, and draws with `RenderBackend::Draw`/`DrawInstanced`. Direct GL state save/restore inside a pass is allowed, because it is layer code.
- Do not add a second friend or widen a private member to make scene code compile. Route scene code through `IUniformSink` or the resource's public CPU API instead.

## Uniform contract: `IUniformSink`

`UniformProvider::Apply(IUniformSink&)` is how scene objects (`Camera`, lights, `GameObject`, `Volume`/`DTIVolume`, `Scene`), `Material`, and the `TypedUniformProvider`/`CompositeUniformProvider` helpers publish uniforms. The sink has `SetBool`/`SetInt`/`SetFloat`/`SetVec2`/`SetVec3`/`SetMat4(name, value)` and `SetTexture(name, const Texture*, unsigned unit)`. `SetTexture` binds the texture when it is non-null and always sets the sampler uniform.

`Shader` is the concrete sink, but only `RenderBackend::Uniforms` hands it out. `Shader::Use()` activates the program and re-applies the shader's own stored (inspector-editable) uniforms via `Apply(*this)`.

Fixed texture units: material slots 0-4 (`PBR::TextureSlot` order), skybox cubemap 5, volume textures `8 + index` (`VolumeTextureSet`), FXAA scene color 0. Instanced draws use units 8-11 for `InstanceBuffer` buffer streams (`InstanceBuffer::kFirstBufferStreamTextureUnit + i`, at most `kMaxBufferStreams` = 4) and 12-15 for per-draw providers (e.g. the SH glyph basis, unit 12). The volume and instanced ranges overlap but never meet in one draw.

## `TextureFormat`

`Texture2D`'s allocation constructor and both `Texture3D` constructors (directly, or through `TextureData3D::format`) take a GL-neutral `TextureFormat` (`R32F`, `RGB32F`, `RGBA32F`, `RGBA8`, `Depth24`). `ToGLTextureFormat` in `render/internal/opengl/TextureGL.h` maps each value to its GL (internal format, format, type) triple, so scene code such as `DTIVolume`/`FloatVolume` never names GL enums.

## Boundary data

- The scene side builds typed render commands (`render/core/RenderCommands.h`: `MeshRenderCommand`/`MeshRenderBatch`, `MeshInstancedRenderCommand`/`MeshInstancedRenderBatch`, `VolumeRenderCommand`, `SkyboxRenderCommand`). It appends them to a `RenderDataStore` through the engine-side `RenderProxy` (`engine/include/scene/extraction/RenderProxy.h`), and the engine's `RenderFrameBuilder` assembles a `RenderFrame`.
- The layer consumes that frame in `Renderer::Draw`.
- `MeshInstancedRenderCommand::instances` is a non-owning `const InstanceBuffer*` owned by the submitting object (a `GameObject` or an `SHGlyphField`). A submitter that needs extra per-draw resources (such as the glyph basis table) adds itself to the command's `frameUniforms` copy, as `Volume` does, instead of adding a command field.
- Postprocessing is gated per frame through `ForwardRenderer::SetPostProcessingEnabled`. The engine runtime sets it each render from the `postprocessing.enabled` engine setting.
- `Renderer::Draw(frame)` renders into whatever framebuffer is bound on entry. `Renderer::Draw(frame, const RenderTarget&)` (`ForwardRenderer`) resizes the renderer to the target, binds the target through `RenderBackend::BindTarget`, and runs the same `Draw`. Postprocessing already composites into the framebuffer bound on entry, so both paths behave the same.

## Threading: the GL thread is wherever the context is current

The layer has no thread of its own and no knowledge of one. "The GL thread" is whichever thread has the context current and calls into the layer: the engine thread in the `Engine`'s threaded mode, or the host's thread in sync mode. Every graphics-API call, every first draw/bind (which triggers lazy uploads), and `CollectGraphicsGarbage`/`ShutdownGraphicsApi` must run there. Construction and destruction of resources are deliberately safe off that thread where noted below.

### Lazy `Geometry` upload

`Geometry` construction is GL-free, so meshes can be built on a preprocessing worker. `Generate()` runs in each concrete ctor, so tube expansion and similar work stay on the CPU thread that builds it. GPU buffers are created on the first `Draw`/`DrawInstanced` through the private `EnsureGpuResident()`, which also re-buffers after `MarkGpuDirty()`. The upload only *reads* `vertices`/`indices`. Once a geometry has been handed to the GL thread, nothing may mutate them, because that would race with the upload and with off-thread readers such as dataset saving.

### Lazy `Texture3D` upload and `TextureData3D`

`TextureData3D` (`render/texture/TextureData3D.h`) is a GL-free CPU payload: `dimensions`, a `format` (`R32F`, `RGB32F` or `RGBA32F` only) and interleaved float `texels` (x fastest, then y, then z). `IsWellFormed()` checks that the dimensions are positive, the format is supported, and the size equals `w*h*d*components`.

`Texture3D` has two constructors:

- **Eager** `(width, height, depth, format, data, linear)`: uploads immediately, so it must run on the GL thread. `FloatVolume` uses it.
- **Lazy** `(TextureData3D, linear)`: GL-free, so it can run on any thread. A malformed payload logs an error and marks the texture failed. Otherwise the texels are kept as pending. `DTIVolume` uses this path; its payloads are packed on the preprocessing worker by `PackDTIVolumeTextures`.

At the first `Bind(unit)` of a pending texture:

1. The texture activates the unit and drains stale `glGetError` state. Draining is required because the upload now happens mid-frame.
2. It checks the size against `GL_MAX_3D_TEXTURE_SIZE`, then creates and uploads the texture, leaving it bound on that unit.
3. It becomes resident, or failed with an error naming the dimensions and format.
4. It frees the CPU texels either way.

A failed or not-yet-resident texture binds 0 on its unit. `IsValid()` is `true` while a texture is pending, so an upload failure surfaces one frame after construction. A texture that is never bound, such as one on a hidden volume, keeps its CPU texels until it is.

### `InstanceBuffer` streams

An `InstanceBuffer` is a set of per-instance streams, all GL-free to fill, so it can be built on a worker and then moved into place on the engine thread:

- **Transforms** (`SetTransforms`, mandatory): one `mat4` per instance at attribute locations 4-7. Its size defines the instance count.
- **Attribute streams** (`AddAttributeStream({location, components})`): tightly packed floats, 1-4 components, at a location in 8-15, with divisor 1.
- **Buffer streams** (`AddBufferStream({samplerName, format, floatsPerInstance})`): a `BufferTexture` (`samplerBuffer`, R32F or RGBA32F only, since RGB32F buffer textures need GL 4.0). The shader indexes it with `gl_InstanceID`. `InstanceBuffer::Apply` binds stream `i` to `samplerName` on unit `8 + i`, and `InstancedMeshGeometryPass` calls it before every instanced draw.

Each stream is filled with `SetStreamData(id, floats)`, which must hold `count × floatsPerInstance` floats. A bad descriptor or stream id logs an error, returns `kInvalidInstanceStream`, and leaves the buffer invalid until `Clear()`. An invalid buffer (empty, wrong sizes, or a descriptor error) is skipped with the pass's `WarningOnce`.

Upload is lazy. `Geometry::DrawInstanced` calls `RenderBackend::EnsureResident` and binds the transform and attribute streams. Buffer textures upload at their first bind. After the draw, `DrawInstanced` **disables every instance location it enabled**, because the VAO belongs to the `Geometry` and is shared with non-instanced draws. Transform and attribute data stay on the CPU after upload, like `Geometry` vertices. Buffer-stream texels are freed after upload, like `Texture3D`.

The setters make no GL calls but have no locking, so they must not race a draw of the same buffer.

`BufferTexture` checks `GL_MAX_TEXTURE_BUFFER_SIZE` (counted in texels) at upload and logs an error when it is exceeded. GL 3.3 only guarantees 65536 texels, so very large instance payloads depend on the driver.

### `GpuDeletionQueue`

Destructors of every GPU-owning resource (`Texture2D/3D/Cube`, `BufferTexture`, `Shader`, `Geometry`, `InstanceBuffer`, `Framebuffer`) may run on any thread. For example, the UI thread can drop the last `shared_ptr` to an inspectable provider. So these destructors never call the graphics API. They call `EnqueueGpuDeletion(kind, handle)` (`render/internal/opengl/GpuDeletionQueue.h`, internal, mutex-guarded), and the GL thread deletes the queued objects in `FlushGpuDeletions()`.

The queue is process-global. That assumes one GL context per process, the same assumption glad's global function table makes. It accepts handles only between `InitializeGraphicsApi` and `ShutdownGraphicsApi`, so destructors that run after the context is gone don't grow it.

### `RenderTarget`

`RenderTarget` (`render/RenderTarget.h`, `engine/src/render/opengl/RenderTarget.cpp`) is an offscreen color + depth target wrapping the private `Framebuffer`. Its constructor is GL-free; everything else runs on the GL thread.

- `Resize(w, h)` reallocates only when the size changes, and ignores a non-positive size.
- `ReadPixelsRGBA8(vector<uint8_t>&)` reads the color attachment with pack alignment 4 and reverses the rows, so the result is tightly packed RGBA8 **top-down**.
- Binding is GPU-only, through `RenderBackend::BindTarget`. That is the only friend path.

The engine runtime owns one `RenderTarget` per screen.

## Initialization and shutdown

`InitializeGraphicsApi(GraphicsProcLoader)` (`render/GraphicsApi.h`, implemented in `engine/src/render/opengl/GraphicsApi.cpp` with `gladLoadGLLoader`) loads the entry points for the current context and starts accepting GPU deletions. The engine runtime calls it from `Engine::InitializeOpenGL` (sync mode) or on the engine thread during `Engine::Start` (threaded mode). The runtime then creates the `ForwardRenderer`, which owns GPU resources, and calls `Renderer::InitializeGLState`.

- **`CollectGraphicsGarbage()`**: GL thread. It flushes the deletion queue. The runtime calls it once per render, before drawing.
- **`ShutdownGraphicsApi()`**: GL thread, after every render resource the caller owns has been destroyed. It does a final flush and then stops accepting deletions. The runtime's teardown order is scene -> screens -> renderer -> `CollectGraphicsGarbage` -> `ShutdownGraphicsApi`.
