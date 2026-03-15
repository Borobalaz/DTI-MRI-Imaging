# Camera Movement Component System

Introduce a `ICameraMovement` interface and a concrete `FpsCameraMovement` implementation, then wire the interface into `Camera` as an optional move component. Follows SOLID: Camera keeps its math responsibility; all input/movement logic is isolated in the component; new movement styles (orbit, RTS) extend without touching Camera.

## Steps

### Phase 1 – New files (parallel)
1. Create `include/ICameraMovement.h` — pure interface, no GLFW dependency:
   - `virtual void Update(float deltaTime, glm::vec3& position, glm::vec3& front, glm::vec3& up) = 0`
   - `virtual ~ICameraMovement() = default`
2. Create `include/FpsCameraMovement.h` — concrete class inheriting `ICameraMovement`:
   - Constructor: `FpsCameraMovement(GLFWwindow* window, float speed = 2.5f, float mouseSensitivity = 0.1f)`
   - Public: `void SetSpeed(float)`, `void SetMouseSensitivity(float)`
   - Private: `GLFWwindow* window`, `float yaw`, `float pitch`, `float lastMouseX`, `float lastMouseY`, `bool firstMouse`, `float speed`, `float sensitivity`
   - Private helpers: `ProcessKeyboard(...)`, `ProcessMouse(...)`
3. Create `src/engine/FpsCameraMovement.cpp`:
   - **Keyboard** (WASD): compute `right = normalize(cross(front, worldUp))`. Move `position` along `front` and `right` scaled by `speed * deltaTime`.
   - **Mouse look**: poll `glfwGetCursorPos` each frame, compute delta from last frame (skip first frame via `firstMouse` flag), multiply by sensitivity, add to `yaw`/`pitch`, clamp `pitch` to `[-89, 89]`.
   - **Front recompute** from yaw/pitch:
     ```
     front.x = cos(radians(yaw)) * cos(radians(pitch))
     front.y = sin(radians(pitch))
     front.z = sin(radians(yaw)) * cos(radians(pitch))
     front = normalize(front)
     ```
   - Initial `yaw = -90.0f` (aligns with default front `(0,0,-1)`), `pitch = 0.0f`.

### Phase 2 – Extend Camera base (depends on Phase 1)
4. Modify `include/Camera.h`:
   - Add `#include <memory>` and forward-declare `ICameraMovement`.
   - Move `front` and `up` protected members here from `PerspectiveCamera`.
   - Add `void SetMoveComponent(std::unique_ptr<ICameraMovement> component)`.
   - Add `protected: void ApplyMoveComponent(float deltaTime)` — calls `moveComponent->Update(...)` if set.
   - Add `private: std::unique_ptr<ICameraMovement> moveComponent`.
5. Modify `src/engine/Camera.cpp`:
   - Initialize `front(0,0,-1)` and `up(0,1,0)` in constructor.
   - Implement `SetMoveComponent` and `ApplyMoveComponent`.

### Phase 3 – Update PerspectiveCamera (depends on Phase 2)
6. Modify `include/PerspectiveCamera.h`:
   - Remove `front` and `up` private members (now in base).
7. Modify `src/engine/PerspectiveCamera.cpp`:
   - Remove `front`/`up` constructor initialization lines (already set by `Camera()`).
   - `Update(float deltaTime)`: call `ApplyMoveComponent(deltaTime)` — this is the only change needed.

### Phase 4 – Scene and main wiring (depends on Phase 2)
8. Modify `include/Scene.h` + `src/engine/Scene.cpp`:
   - Add `Camera& GetCamera()` returning the owned `PerspectiveCamera` as a `Camera&` reference.
9. Modify `src/main.cpp`:
   - After `scene.Init()`: create `FpsCameraMovement(window)` with `std::make_unique`, call `scene.GetCamera().SetMoveComponent(std::move(...))`.
   - Capture cursor before the game loop: `glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED)`.

## Relevant files
- `include/ICameraMovement.h` — new interface (no GLFW, SRP-clean)
- `include/FpsCameraMovement.h` — new FPS concrete; only file that depends on GLFW
- `src/engine/FpsCameraMovement.cpp` — WASD + mouse-look implementation
- `include/Camera.h` — gains `front`, `up`, `moveComponent`, `SetMoveComponent`, `ApplyMoveComponent`
- `src/engine/Camera.cpp` — constructor + component delegation implementation
- `include/PerspectiveCamera.h` — remove `front`/`up` (moved to base)
- `src/engine/PerspectiveCamera.cpp` — remove duplicate init; call `ApplyMoveComponent` in `Update()`
- `include/Scene.h` — add `Camera& GetCamera()`
- `src/engine/Scene.cpp` — implement `GetCamera()`
- `src/main.cpp` — wire `FpsCameraMovement` after scene init, capture cursor

## Verification
1. `./build.ps1` — verify compile/link succeeds with no breaking changes to existing uniform path.
2. `./build-and-run.ps1` — camera moves with WASD; mouse look pivots view; model scene reacts to changed view.
3. Verify calling `SetMoveComponent(nullptr)` (or not calling it at all) still renders scene without movement, uncrashed.
4. Verify yaw/pitch clamping prevents camera flip past ±89°.

## Decisions
- `ICameraMovement::Update` mutates `position`, `front`, `up` by reference — minimal interface, no coupling to `Camera` class hierarchy (ISP/DIP).
- `front`/`up` moved to `Camera` base — justified because every concrete camera needs a look-direction; PerspectiveCamera was the only user and they were redundant there.
- `FpsCameraMovement` holds `GLFWwindow*` directly — acceptable at the input boundary; abstracting input further is a separate future concern.
- Cursor capture done in `main.cpp`, not in the movement component — keeps platform code at the entry point, not in the engine.
- Excluded from this iteration: orbit mode, RTS drag-pan, gamepad input, scroll-wheel zoom.
