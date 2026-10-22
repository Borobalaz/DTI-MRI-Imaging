# Render Layer Architecture

## Purpose

The render layer is the `engine_render` CMake STATIC target, built from `engine/render/src`. It is the only code in the repository that includes `<glad/glad.h>` or calls the graphics API. It owns the GPU resource classes, the uniform contract, the renderer, and the render/postprocessing passes. Scene-side code in `engine/src` and `core/` uses the layer only through the public, GL-free headers under `engine/render/include/Render/`.

Build evidence: `CMakeLists.txt` (`add_library(engine_render STATIC ...)`). `glad::glad` and `OpenGL::GL` are linked `PRIVATE` to `engine_render`, and `engine_render` is linked `PRIVATE` into the `engine` DLL.

## Dependency direction

```
app_qt  -->  engine (SHARED)  -->  engine_render (STATIC)  -->  engine_foundation (STATIC)
```

- `engine_foundation` (`engine/foundation/`) holds the dependency-free utilities that both the layer and the engine need: `EngineLog` (`Log/EngineLog.h`) and `StableTypeId` (`RenderCore/StableTypeId.h`).
- `engine_render` depends on `engine_foundation` and `glm`, and has `glad`/OpenGL as private dependencies. It never includes headers from `engine/src/include` or `core/`.
- `engine` (scene, volumes, concrete geometries, extraction, the `Engine` facade) depends on the layer's public headers. It never includes `engine/render/src`.

## Public vs private split

| Location | Visibility | Contents |
|---|---|---|
| `engine/render/include/Render/` | PUBLIC include dir of `engine_render` (included as `"Render/..."`) | GL-free API: `GraphicsApi.h`, `TextureFormat.h`, `Shader.h`, `Material.h`, `PBRTextureSlots.h`, `VolumeTextureSet.h`, `Texture/{Texture,Texture2D,Texture3D,TextureCube}.h`, `Geometry/{Geometry,InstanceBuffer}.h`, `Uniform/{IUniformSink,UniformProvider,CompositeUniformProvider,TypedUniformProvider}.h`, `Core/{RenderCommands,RenderDataStore,RenderFrame,RendererDescriptor}.h`, `Renderer/{Renderer,ForwardRenderer,RenderPass}.h`, `Postprocessing/IPostProcessingPass.h` |
| `engine/render/src/` | PRIVATE include dir of `engine_render` (included relative to `src`, e.g. `"RenderBackend.h"`, `"OpenGL/Framebuffer.h"`, `"Passes/VolumePass.h"`) | `RenderBackend`, plus the implementations of `Material`, `VolumeTextureSet` and the `Uniform/` providers |
| `engine/render/src/OpenGL/` | private | The OpenGL implementations of the resources: `GraphicsApi.cpp` (glad loader), `Shader.cpp`, `Texture*.cpp` with `TextureGL.h` (the `TextureFormat` to GL mapping and the stb channel-format helper), `Geometry.cpp`, `InstanceBuffer.cpp`, `Framebuffer.h/.cpp`, `FullscreenTriangleGeometry.h/.cpp` |
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
```

| Class | Public (CPU) API | Private, reachable only through `RenderBackend` |
|---|---|---|
| `Shader` | constructor, `GetId`, `IsValid`, `ReloadIfChanged`, `operator[]` (stored uniforms), `GetStoredUniforms`, `Apply(IUniformSink&)` | `programId`, `Use`, the `IUniformSink` setters, uniform reflection (`GetUniformInfo(s)`, `HasUniform`) |
| `Texture`, `Texture2D`, `Texture3D`, `TextureCube` | constructors, `IsValid` | `Bind`, `GetId` (private virtuals) |
| `Geometry` | `Generate`, `Upload`, `Destroy`, `ComputeNormals`, `GetVertices`/`GetIndices`; protected `vertices`/`indices` and draw-mode setters | `Draw`, `DrawInstanced`, `VAO`/`VBO`/`EBO` |
| `InstanceBuffer` | `Upload`, `Reset`, `IsValid`, `GetInstanceCount` | `GetHandle` |

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

`Texture2D`'s allocation constructor and `Texture3D`'s constructor take a GL-neutral `TextureFormat` (`R32F`, `RGB32F`, `RGBA32F`, `RGBA8`, `Depth24`). `ToGLTextureFormat` in `OpenGL/TextureGL.h` maps each value to its GL (internal format, format, type) triple, so scene code such as `DTIVolume`/`FloatVolume` never names GL enums.

## Boundary data

- The scene side builds typed render commands (`Render/Core/RenderCommands.h`: `MeshRenderCommand`/`MeshRenderBatch`, `MeshInstancedRenderCommand`/`MeshInstancedRenderBatch`, `VolumeRenderCommand`, `SkyboxRenderCommand`). It appends them to a `RenderDataStore` through the engine-side `RenderProxy` (`engine/src/include/Renderer/RenderProxy.h`), and the engine's `RenderFrameBuilder` assembles a `RenderFrame`.
- The layer consumes that frame in `Renderer::Draw`.
- `MeshInstancedRenderCommand::instances` is a non-owning `const InstanceBuffer*` owned by the submitting `GameObject`.
- Postprocessing is gated per frame through `ForwardRenderer::SetPostProcessingEnabled`. `Engine::Render` sets it from the `postprocessing.enabled` engine setting.

## Initialization

`Engine::InitializeOpenGL(Engine::GLProcLoader)` forwards to `InitializeGraphicsApi(GraphicsProcLoader)` (`Render/GraphicsApi.h`, implemented in `OpenGL/GraphicsApi.cpp` with `gladLoadGLLoader`). It then calls `Renderer::InitializeGLState`.
