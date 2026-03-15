## Plan: SOLID UniformProvider Architecture

Build a typed, reflection-aware uniform system centered on `UniformProvider`, with composable providers (model, camera, material/custom) and shader-side uniform metadata caching. This keeps high flexibility for arbitrary uniforms while preserving interface segregation, single responsibility, and open/closed extensibility.

**Steps**
1. Phase 1 - Reflection Foundation
2. Add uniform reflection metadata to `Shader` (uniform name, GL type, size, location) and cache it after successful program link; expose query APIs to retrieve all uniforms and lookup by name.
3. Add location caching and type-validation helpers in `Shader` so setter calls can validate expected GL type and fail predictably in debug logs. *depends on 2*
4. Phase 2 - Typed Provider Core
5. Introduce a new concrete provider (for example `TypedUniformProvider`) implementing `UniformProvider` with typed setter methods and internal `std::variant` storage for supported uniform value types (`bool`, `int`, `float`, `glm::vec3`, `glm::mat4`, and optional texture slot integers). *depends on 2*
6. Implement `Apply(Shader&)` to iterate stored values, verify each name exists in reflection data, validate type compatibility, then call the matching `Shader` setter; skip or warn on unknown uniforms to avoid hard crashes. *depends on 5*
7. Phase 3 - Composition and Scene Integration
8. Add `CameraUniformProvider` to publish view/projection uniforms from `Camera`/`PerspectiveCamera`; keep camera math in camera classes and only mapping concerns in provider. *parallel with 5 after 2*
9. Add `CompositeUniformProvider` that aggregates multiple `UniformProvider` instances and applies them deterministically (stable order). *depends on 5 and 8*
10. Refactor render path to use provider composition consistently: remove duplicated manual model uniform logic in `GameObject::Draw`, ensure model/view/projection are all provided through providers. *depends on 9*
11. Phase 4 - Validation and Hardening
12. Add unit/integration checks (or debug assertions/logging if no test framework) covering: reflected uniform discovery, type mismatch handling, missing uniform behavior, and multi-provider composition order. *depends on 10*
13. Validate with existing scene (`Triangle` + shader pipeline) and confirm no regressions in rendering output and startup flow.

**Relevant files**
- `c:/Projects/Connectomics-Imaging/include/UniformProvider.h` - Existing abstraction to preserve; ensure descendants remain substitutable.
- `c:/Projects/Connectomics-Imaging/src/engine/UniformProvider.cpp` - Implement or host shared provider internals as needed.
- `c:/Projects/Connectomics-Imaging/include/Shader.h` - Add reflection metadata structures and public query API.
- `c:/Projects/Connectomics-Imaging/src/engine/Shader.cpp` - Implement reflection, caching, type checks, and setter integration.
- `c:/Projects/Connectomics-Imaging/include/Camera.h` - Reuse camera interface for provider inputs.
- `c:/Projects/Connectomics-Imaging/src/engine/PerspectiveCamera.cpp` - Reuse matrix computation behavior for camera provider wiring.
- `c:/Projects/Connectomics-Imaging/include/Geometry.h` - Keep model transform source and align with provider-based application.
- `c:/Projects/Connectomics-Imaging/src/engine/Geometry.cpp` - Ensure model uniform responsibility is not duplicated elsewhere.
- `c:/Projects/Connectomics-Imaging/src/engine/GameObject.cpp` - Remove direct uniform setting and delegate to providers/composition.
- `c:/Projects/Connectomics-Imaging/src/engine/Scene.cpp` - Integrate provider setup/application flow.
- `c:/Projects/Connectomics-Imaging/shaders/vertex.glsl` - Verify expected uniform names/types (`model`, `view`, `projection`) match provider outputs.

**Verification**
1. Build with `build.ps1` and confirm no compile/link errors after introducing new provider/reflection APIs.
2. Run app with `build-and-run.ps1` and verify geometry still renders correctly with provider-driven uniforms.
3. Add a controlled type mismatch case (for example setting `mat4` uniform with float) and verify the runtime warning/error path triggers cleanly.
4. Add a missing-uniform case and verify behavior is non-fatal and observable in logs.
5. Confirm composite provider order determinism by setting same uniform from two providers and asserting documented precedence.

**Decisions**
- Chosen direction: typed public provider API with internal variant storage (hybrid safety/flexibility).
- Included now: reflection-capable base provider plus camera and composite providers.
- SOLID mapping: `UniformProvider` for polymorphism (LSP), provider-per-domain responsibility (SRP), composition to extend without modifying existing providers (OCP), narrow interfaces for uniform application (ISP), and dependency on abstractions (`UniformProvider`) in render flow (DIP).
- Excluded now: UBO/SSBO refactor, material editor tooling, and hot-reload reflection invalidation beyond normal relink lifecycle.

**Further Considerations**
1. Name policy recommendation: standardize uniforms as `model`, `view`, `projection` and keep this contract documented in shader conventions.
2. Performance recommendation: reflection and location maps should be built once at link time, never per-frame.
3. Error policy recommendation: debug builds log detailed mismatch info; release builds downgrade to lightweight warnings or silent skips for resilience.
