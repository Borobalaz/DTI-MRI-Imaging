# Uniform Providers Sequence Diagram

This diagram illustrates the flow of uniform providers and uniform application during a scene render before draw calls.

## Sequence: Frame Rendering with Uniform Providers

```mermaid
sequenceDiagram
    participant Render as Render Loop
    participant Scene
    participant CompUniforms as CompositeUniformProvider<br/>(frameUniforms)
    participant Camera
    participant Light
    participant GameObject
    participant GameObjectComp as CompositeUniformProvider<br/>(compositeProvider)
    participant Mesh
    participant Material
    participant Shader

    Render->>Scene: Render()
    
    Note over Scene: Setup Phase
    Scene->>CompUniforms: ClearProviders()
    Scene->>CompUniforms: AddProvider(*this)
    Note over Scene: Scene contributes lightCount
    
    alt Camera exists
        Scene->>CompUniforms: AddProvider(*camera)
        Note over Camera: Camera contributes view & projection matrices
    end
    
    loop For each enabled Light i
        Scene->>Light: SetUniformIndex(i)
        Scene->>CompUniforms: AddProvider(*light)
        Note over Light: Light contributes position, color uniforms
    end
    
    Note over Scene: Draw Phase
    loop For each GameObject
        Scene->>GameObject: Draw(frameUniforms)
        
        alt GameObject visible and has meshes
            GameObject->>GameObjectComp: Create CompositeUniformProvider
            GameObject->>GameObjectComp: AddProvider(frameUniforms)
            Note over GameObjectComp: Adds all scene uniforms
            GameObject->>GameObjectComp: AddProvider(*this)
            Note over GameObject: GameObject contributes model matrix
            
            loop For each Mesh
                GameObject->>Mesh: Draw(compositeProvider)
                
                Mesh->>Material: Bind()
                Mesh->>Material: GetShader()
                Material-->>Mesh: Shader&
                
                Note over Mesh: Apply Phase
                Mesh->>GameObjectComp: Apply(shader)
                
                GameObjectComp->>CompUniforms: Apply(shader)
                CompUniforms->>Scene: Apply(shader)
                Scene-->>Shader: SetInt(lightCount, ...)
                
                CompUniforms->>Camera: Apply(shader)
                Camera-->>Shader: SetMat4(view, ...)<br/>SetMat4(projection, ...)
                
                loop For each Light
                    CompUniforms->>Light: Apply(shader)
                    Light-->>Shader: SetVec3(position, ...)<br/>SetVec3(color, ...)
                end
                
                GameObjectComp->>GameObject: Apply(shader)
                GameObject-->>Shader: SetMat4(model, ...)
                
                Note over Shader: All uniforms now applied
                Mesh->>Mesh: geometry->Draw(shader)
                Mesh->>Shader: Draw Call ✓
            end
        end
    end
    
    loop For each Volume
        Scene->>Volume: Draw(frameUniforms)
        Note over Volume: Same uniform application pattern as GameObject
    end
```

## Component Roles

### Scene (UniformProvider)
- **Role**: Frame-level uniform aggregator and provider
- **Contributes**: 
  - `lightCount`: Number of active lights
- **Holds**: `frameUniforms` (CompositeUniformProvider)

### CompositeUniformProvider (at frame level)
- **Role**: Aggregates multiple uniform providers for the frame
- **Members**:
  - Scene (light count)
  - Camera (view, projection matrices)
  - Multiple Lights (light properties by index)
- **Responsibility**: Iterate providers and call Apply() on each

### Camera (UniformProvider)
- **Role**: Provides view and projection matrices
- **Contributes**: 
  - `view`: View matrix
  - `projection`: Projection matrix

### Light (UniformProvider)
- **Role**: Provides light-indexed uniforms
- **Contributes** (per light index i):
  - `lights[i].position`, `lights[i].color`, `lights[i].intensity`, etc.

### GameObject (UniformProvider, IDrawable)
- **Role**: 
  - Receives frame uniforms
  - Composes own model-specific uniforms
  - Passes combined uniforms to meshes
- **Contributes**:
  - `model`: Model transformation matrix
- **Creates**: Local CompositeUniformProvider combining frame + self

### Mesh (Drawable unit)
- **Role**: Final uniform applicator before geometry draw
- **Steps**:
  1. Bind material (sets shader)
  2. Call `uniformProvider.Apply(shader)`
  3. Draw geometry with shader

### Shader (Uniform sink)
- **Role**: Receives individual uniform values via Apply()
- **Methods called**:
  - `SetBool()`, `SetInt()`, `SetFloat()`, `SetVec3()`, `SetMat4()`
- **Precondition**: `HasUniform(name)` check before setting

## TypedUniformProvider Detail

When `TypedUniformProvider::Apply()` is called:
1. Iterates over stored `std::map<std::string, UniformValue>`
2. For each uniform:
   - Checks if shader has the uniform with `HasUniform(name)`
   - Uses `std::visit()` to dispatch on variant type
   - Calls appropriate shader method: `SetBool()`, `SetInt()`, `SetFloat()`, `SetVec3()`, `SetMat4()`

## Key Patterns

### Composite Pattern (Decorator-like)
- `CompositeUniformProvider` aggregates multiple providers
- Calling `Apply()` delegates to all member providers
- Enables flexible combination of uniform sources

### Chain of Responsibility
- Uniforms flow through: Scene → GameObject → Mesh → Shader
- Each level can add/override uniforms
- Shader is the final consumer

### Double Dispatch (via std::visit)
- `TypedUniformProvider::Apply()` uses visitor pattern
- Handles polymorphic variant types without runtime type checking
- Type-safe uniform dispatch to shader methods

## Visibility & Filtering

- **GameObject visibility**: Scene checks `gameObject->visible` before drawing
- **Light filtering**: Only enabled lights are added to `frameUniforms`
- **Uniform existence**: Shader checks `HasUniform()` before setting to avoid errors
