# Volume Class Hierarchy Diagram

## Complete Inheritance Structure

```mermaid
classDiagram
    %% Base Interfaces and Classes
    class UniformProvider {
        <<interface>>
        +Apply(shader: Shader&)*
    }

    class IDrawable {
        <<interface>>
        +Draw(frameUniforms: UniformProvider&)*
        visible: bool
        +SetVisible(isVisible: bool)
    }

    class InspectProvider {
        <<interface>>
        +GetInspectDisplayName()* string
        +GetInspectFields()* vector~shared_ptr~IInspectWidget~~
        +HasVisibility()* bool
        +IsVisible()* bool
        +CastRay(rayOrigin: vec3, rayDirection: vec3)* optional~float~
    }

    class Geometry {
        <<abstract>>
        +Generate()*
    }

    class VolumeGeometry {
        +VolumeGeometry()
        +Generate() override
    }

    %% Data Structures
    class VolumeData {
        -dimensions: glm::ivec3
        -spacing: glm::vec3
        -voxels: vector~float~
        +VolumeData()
        +VolumeData(filePath: string)
        +VolumeData(width: int, height: int, depth: int, spacing: vec3)
        +Resize(width: int, height: int, depth: int)
        +GetWidth() int
        +GetHeight() int
        +GetDepth() int
        +GetDimensions() const ivec3&
        +GetSpacing() const vec3&
        +GetVoxelCount() size_t
        +GetVoxels() vector~float~&
        +GetValue(coord: vec3) float
        +FlatIndex(x: int, y: int, z: int, width: int, height: int)$ size_t
    }

    class VolumeMetadata {
        +dimensions: glm::ivec3
        +spacing: glm::vec3
    }

    class VolumeTextureSet {
        -textures: vector~shared_ptr~Texture3D~~
        -kVolumeTextureBaseUnit: unsigned int$ = 8
        +AddTexture(texture: shared_ptr~Texture3D~)
        +Bind(shader: Shader&, uniformBaseName: string)
        +Bind(shader: Shader&, textureIndex: int, uniformBaseName: string, uniformSubName: string)
        +IsValid() bool
        +Size() size_t
    }

    class DTIVolumeChannels {
        +Dxx: VolumeData
        +Dyy: VolumeData
        +Dzz: VolumeData
        +Dxy: VolumeData
        +Dxz: VolumeData
        +Dyz: VolumeData
        +EVx: VolumeData
        +EVy: VolumeData
        +EVz: VolumeData
        +L1: VolumeData
        +L2: VolumeData
        +L3: VolumeData
        +FA: VolumeData
        +MD: VolumeData
        +AD: VolumeData
        +RD: VolumeData
        +Mask: VolumeData
    }

    %% Core Volume Class
    class Volume {
        <<abstract>>
        -id: string
        -visible: bool
        -position: glm::vec3
        -rotation: glm::vec3
        -scale: glm::vec3
        -dimensions: glm::ivec3
        -spacing: glm::vec3
        #geometry: shared_ptr~VolumeGeometry~
        #shader: shared_ptr~Shader~
        #textureSet: VolumeTextureSet
        +Apply(shader: Shader&) override
        +Draw(frameUniforms: UniformProvider&) override
        +IsValid() bool
        +getShader() const shared_ptr~Shader~&
        +GetTextureSet() const VolumeTextureSet&
        +GetPosition() const vec3&
        +GetRotation() const vec3&
        +GetScale() const vec3&
        +SetPosition(newPosition: vec3)
        +SetRotation(newRotation: vec3)
        +SetScale(newScale: vec3)
        +GetId() const string&
        +HasVisibility() bool override
        +IsVisible() bool override
        +SetVisible(newVisible: bool)
        +GetInspectDisplayName() string override
        +GetInspectFields() vector~shared_ptr~IInspectWidget~~ override
        +CastRay(rayOrigin: vec3, rayDirection: vec3) optional~float~ override
        -BuildModelMatrix() mat4
        #Volume(id: string, dimensions: ivec3, spacing: vec3, shader: shared_ptr~Shader~)
    }

    %% Derived Classes
    class FloatVolume {
        <<final>>
        +FloatVolume(id: string, volumeData: VolumeData&, shader: shared_ptr~Shader~)
    }

    class DTIVolume {
        <<final>>
        -channels: DTIVolumeChannels
        -selectedChannel: int = 0
        -selectedRenderMode: int = 0
        -sliceZValue: float = 0.5f
        -renderModes: vector~RenderMode~
        -RenderMode: struct
        +DTIVolume(id: string, channels: DTIVolumeChannels, shader: shared_ptr~Shader~)
        +Apply(shader: Shader&) override
        +Draw(frameUniforms: UniformProvider&) override
        +GetMajorEigenVectorAt(voxelCoord: ivec3, outVector: vec3&)
        +GetSelectedRenderModeIndex() int
        +GetSelectedChannelIndex() int
        +SetSelectedRenderModeIndex(modeIndex: int) bool
        +SetSelectedChannelIndex(channelIndex: int)
        +RegisterShadersWithScene(scene: Scene*)
        +GetInspectFields() vector~shared_ptr~IInspectWidget~~ override
        -InitializeRenderModes()
        -SetActiveRenderMode(modeIndex: int) bool
        -GetActiveRenderMode() const RenderMode*
        -SelectedChannelIndex: enum
    }

    %% Relationships
    Volume --|> UniformProvider : implements
    Volume --|> IDrawable : implements
    Volume --|> InspectProvider : implements

    FloatVolume --|> Volume : inherits
    DTIVolume --|> Volume : inherits

    Volume --> VolumeGeometry : owns
    Volume --> Shader : owns
    Volume --> VolumeTextureSet : owns
    
    VolumeGeometry --|> Geometry : inherits

    DTIVolume --> DTIVolumeChannels : owns
    DTIVolumeChannels --> VolumeData : contains 17x

    VolumeData --> VolumeMetadata : uses
    VolumeTextureSet --> Texture3D : stores
```

---

## Class Details

### **Volume (Abstract Base Class)**
- **Purpose**: Base class for all volume renderers in the scene
- **Responsibilities**:
  - Manages volume transform (position, rotation, scale)
  - Owns volume geometry and shader
  - Provides uniform application and draw calls
  - Manages texture bindings through VolumeTextureSet
  
- **Key Properties**:
  - `id`: Unique identifier
  - `visible`: Visibility toggle
  - `dimensions`, `spacing`: Volume grid properties
  - `geometry`: VolumeGeometry instance (cube)
  - `shader`: Active shader for rendering
  - `textureSet`: 3D textures bound to shader

- **Protected Constructor**: Only subclasses can instantiate (enforces proper initialization)

### **FloatVolume (Concrete Class)**
- **Purpose**: Simple scalar volume renderer
- **Use Case**: Single-channel volumetric data (e.g., density, intensity maps)
- **Data**: One VolumeData containing float voxels
- **Inheritance**: Minimal; inherits all base Volume functionality

### **DTIVolume (Concrete Class)**
- **Purpose**: Specialized renderer for DTI (Diffusion Tensor Imaging) data
- **Data**: 17 separate VolumeData channels:
  - **6 tensor components**: Dxx, Dyy, Dzz, Dxy, Dxz, Dyz
  - **3 eigenvector components**: EVx, EVy, EVz
  - **3 eigenvalues**: L1, L2, L3
  - **4 scalar metrics**: FA, MD, AD, RD
  - **1 mask**: Skull extraction mask

- **Key Features**:
  - Multiple render modes (e.g., Z-slicing, MIP, etc.)
  - Channel selection (0-15) for visualization
  - Slice parameter for orthogonal plane rendering
  - Query eigenvector at voxel location
  - Hot-reload shader registration with Scene

- **Render Modes**: Allows different shader programs for different visualization techniques

---

## VolumeData Structure

```
VolumeData
├── Metadata
│   ├── dimensions: ivec3 (width, height, depth)
│   └── spacing: vec3 (physical size of each voxel)
├── Voxels: vector<float>
│   └── Linear storage: [x0y0z0, x1y0z0, ..., xWyHzD]
└── Methods
    ├── Load from file
    ├── Resize grid
    ├── Query single voxel
    └── Calculate flat index from 3D coordinates
```

---

## DTIVolumeChannels Structure

```
DTIVolumeChannels
├── Tensor Components (6)
│   ├── Dxx, Dyy, Dzz (diagonal)
│   └── Dxy, Dxz, Dyz (off-diagonal)
├── Eigensystem (6)
│   ├── EVx, EVy, EVz (principal eigenvector)
│   └── L1, L2, L3 (eigenvalues)
├── Derived Scalars (4)
│   ├── FA (Fractional Anisotropy)
│   ├── MD (Mean Diffusivity)
│   ├── AD (Axial Diffusivity)
│   └── RD (Radial Diffusivity)
└── Mask (1)
    └── Binary skull extraction mask
```

**Total**: 17 channels, each a separate VolumeData instance

---

## Data Flow: Volume Creation

### FloatVolume
```
VolumeData (single scalar)
    ↓
FloatVolume(id, volumeData, shader)
    ├── Set dimension/spacing from volumeData
    ├── Create VolumeGeometry (unit cube)
    ├── Bind volumeData as Texture3D
    └── Store in VolumeTextureSet (1 texture)
```

### DTIVolume
```
DTIVolumeChannels (17 VolumeData instances)
    ↓
DTIVolume(id, channels, shader)
    ├── Set dimension/spacing from channels (e.g., channels.Dxx)
    ├── Create VolumeGeometry (unit cube)
    ├── Bind each channel as Texture3D
    │   ├── volumeTextures[0] = {Dxx, Dyy, Dzz} (RGB)
    │   ├── volumeTextures[1] = {Dxy, Dxz, Dyz} (RGB)
    │   ├── volumeTextures[2] = {EVx, EVy, EVz} (RGB)
    │   ├── volumeTextures[3] = {L1, L2, L3} (RGB)
    │   └── volumeTextures[4] = {FA, MD, AD, RD} (RGBA)
    ├── Store in VolumeTextureSet (5 textures packed)
    └── Initialize render modes (Z-slice, MIP, etc.)
```

---

## Interface Implementations

### UniformProvider
- **Apply(Shader&)**: Binds volume-specific uniforms (dimensions, spacing, slice parameters)

### IDrawable
- **Draw(frameUniforms)**: Renders the volume using geometry and shader

### InspectProvider
- **HasVisibility()**: Returns true (volumes are inspectable)
- **IsVisible()**: Returns current visibility state
- **CastRay()**: For UI picking (ray-volume intersection)
- **GetInspectFields()**: Exposes UI widgets for transform, render mode, channel selection

---

## Key Patterns

### **Template Method Pattern**
- `Volume::Draw()` calls protected methods that subclasses can override
- Default implementation works for FloatVolume
- DTIVolume overrides to handle multiple render modes

### **Uniform Provider Pattern**
- Each volume is a UniformProvider
- Scene passes frameUniforms to volume
- Volume combines frame + self uniforms, passes to shader

### **Protected Constructor**
- Prevents direct Volume instantiation
- Ensures texture setup in derived class constructors

---

## Suggested Extensions

To add a new volume type (e.g., VectorVolume, LabelVolume):

```cpp
class YourVolume final : public Volume {
public:
  explicit YourVolume(const std::string& id,
                      const YourData& data,
                      std::shared_ptr<Shader> shader)
    : Volume(id, data.GetDimensions(), data.GetSpacing(), shader),
      data(data) {
    // Bind your data to VolumeTextureSet
  }

  // Override Draw/Apply if custom rendering needed
  void Draw(const UniformProvider& frameUniforms) const override;
  void Apply(Shader& shader) const override;

private:
  YourData data;
};
```
