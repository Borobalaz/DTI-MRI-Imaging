# DTI-MRI-Imaging
Author: Boroczky Balazs

## Developer Environment Setup

This project is a C++17 OpenGL application built with CMake and dependencies managed through vcpkg.

### 1. Prerequisites

- OS: Windows (PowerShell scripts in this repository target Windows)
- Compiler: Visual Studio 2022 with MSVC (Desktop development with C++)
- CMake: 3.20 or newer
- Git
- vcpkg

You can verify tools from a terminal:

```powershell
cmake --version
git --version
cl
```

### 2. Install and Bootstrap vcpkg

This repository expects vcpkg at `C:/vcpkg` by default.

```powershell
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd C:\vcpkg
.\bootstrap-vcpkg.bat
```

Install required libraries for `x64-windows`:

```powershell
cd C:\vcpkg
.\vcpkg.exe install glm:x64-windows glad:x64-windows assimp:x64-windows stb:x64-windows
```

Install Qt 6 (MSVC 2022 64-bit) through the Qt Online Installer and set `QT_ROOT` to your installation, for example:

```powershell
$env:QT_ROOT="C:/Qt/6.11.0/msvc2022_64"
```

### 3. Toolchain and Dependency Notes

`CMakeLists.txt` requires these packages:

- `OpenGL`
- `glm`
- `glad`
- `assimp`
- `Qt6` (`Core`, `Gui`, `OpenGL`, `OpenGLWidgets`)
- `stb_image.h` (provided by vcpkg package `stb`)
- Optional: `ITK` for broad medical volume format support (NIfTI, NRRD, MetaImage, Analyze, DICOM series)

The build script (`build.ps1`) configures CMake with:

```text
-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

If your vcpkg location differs, either:

- Set `CMAKE_TOOLCHAIN_FILE` when running CMake manually, or
- Update `build.ps1` and `CMakeLists.txt` to your vcpkg path.

## Build and Run

From repository root:

```powershell
./build.ps1
```

Build configurations:

```powershell
./build.ps1 Debug
./build.ps1 Release
```

Build and run in one command:

```powershell
./build-and-run.ps1
```

## Manual CMake Build (Alternative)

If you prefer plain CMake commands:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

### Build-Time Discovery Overrides

If your data is not organized in subject/session folders, pass explicit paths directly through `DtiVolumeScene::LoadDataset(...)` from `src/app/main.cpp`.

## Output

Expected executable path:

```text
build/Release/app_qt.exe
```

## Volume File Support

- Native format: `VXA1` (`.vxa`) for scalar and matrix data.
- With ITK installed, loader also accepts common medical imaging formats:
	- NIfTI (`.nii`, `.nii.gz`)
	- NRRD (`.nrrd`, `.nhdr` + raw payload)
	- MetaImage (`.mha`, `.mhd` + raw payload)
	- Analyze (`.hdr` + `.img`)
	- DICOM series (directory containing slices)

Install ITK via vcpkg to enable this automatically:

```powershell
cd C:\vcpkg
.\vcpkg.exe install itk:x64-windows
```

## Class Diagrams

### Drawable Layer

This diagram shows how drawable scene entities (`GameObject`, `Volume` variants, and `Skybox`) relate to the core rendering/inspection contracts and shared rendering resources.

Source: [docs/drawable-class-diagram.mmd](docs/drawable-class-diagram.mmd)

```mermaid
classDiagram
		class IDrawable {
				<<interface>>
				-bool visible
				+Draw(frameUniforms: UniformProvider) const void
				+SetVisible(isVisible) void
		}

		class UniformProvider {
				<<abstract>>
				+Apply(shader: Shader&) const void
		}

		class IInspectable {
				<<interface>>
				+CollectInspectableFields(out, groupPrefix) void
				+CollectInspectableNodes(out, nodePrefix) void
		}

		class GameObject {
				-vector~shared_ptr~Mesh~~ meshes
				+glm::vec3 position
				+glm::vec3 rotation
				+glm::vec3 scale
				+AddMesh(mesh) void
				+Update(deltaTime) void
				+Draw(frameUniforms: UniformProvider) const void
				+Apply(shader: Shader&) const void
				+CollectInspectableFields(out, groupPrefix) void
				-BuildModelMatrix() glm::mat4
		}

		class Volume {
				<<abstract>>
				+glm::vec3 position
				+glm::vec3 rotation
				+glm::vec3 scale
				#glm::ivec3 dimensions
				#glm::vec3 spacing
				#shared_ptr~VolumeGeometry~ geometry
				#shared_ptr~Shader~ shader
				+Draw(frameUniforms: UniformProvider) const void
				+Apply(shader: Shader&) const void
				+IsValid() const bool
				+CollectInspectableFields(out, groupPrefix) void
				+CollectInspectableNodes(out, nodePrefix) void
				#GetTextureSet() const VolumeTextureSet&
				#BuildModelMatrix() glm::mat4
		}

		class FloatVolume {
				-VolumeTextureSet textureSet
				+GetTextureSet() const VolumeTextureSet&
		}

		class UInt16Volume {
				-VolumeTextureSet textureSet
				+GetTextureSet() const VolumeTextureSet&
		}

		class UInt8Volume {
				-VolumeTextureSet textureSet
				+GetTextureSet() const VolumeTextureSet&
		}

		class Mat3Volume {
				-VolumeTextureSet textureSet
				+GetTextureSet() const VolumeTextureSet&
		}

		class Skybox {
				-shared_ptr~TextureCube~ cubemap
				-shared_ptr~CubeGeometry~ geometry
				-shared_ptr~Shader~ shader
				+Draw(camera: Camera) const void
				+IsValid() const bool
		}

		IDrawable <|-- GameObject
		UniformProvider <|-- GameObject
		IInspectable <|-- GameObject

		IDrawable <|-- Volume
		UniformProvider <|-- Volume
		IInspectable <|-- Volume

		Volume <|-- FloatVolume
		Volume <|-- UInt16Volume
		Volume <|-- UInt8Volume
		Volume <|-- Mat3Volume

		Skybox ..> Camera : uses
```

### Graphics Pipeline Root

This diagram captures the high-level scene orchestration path, from frame-level uniforms and camera movement down to mesh/material/shader/texture composition and volume rendering roots.

Source: [docs/graphics-pipeline-root-class-diagram.mmd](docs/graphics-pipeline-root-class-diagram.mmd)

```mermaid
classDiagram

class Scene

class UniformProvider {
	<<abstract>>
	+Apply(shader: Shader&) const
}
class IDrawable {
	<<interface>>
	+Draw(frameUniforms: UniformProvider) const
}
class IInspectable {
	<<interface>>
	+CollectInspectableFields(out, groupPrefix)
	+CollectInspectableNodes(out, nodePrefix)
}

class Camera {
	<<abstract>>
}
class PerspectiveCamera
class BaseMovement {
	<<abstract>>
	+Update(deltaTime, position, front, up)
}

class Light {
	<<abstract>>
}
class GameObject
class Volume {
	<<abstract>>
}

class Mesh
class Material
class Shader
class Geometry {
	<<abstract>>
}
class Texture {
	<<abstract>>
}

class CompositeUniformProvider

%% Root contracts
UniformProvider <|-- Camera
UniformProvider <|-- Light
UniformProvider <|-- Shader
UniformProvider <|-- CompositeUniformProvider
UniformProvider <|-- Scene
UniformProvider <|-- GameObject
UniformProvider <|-- Volume

IDrawable <|-- GameObject
IDrawable <|-- Volume
IInspectable <|-- Scene
IInspectable <|-- Light
IInspectable <|-- GameObject
IInspectable <|-- Shader
IInspectable <|-- Volume

%% Camera branch
Camera <|-- PerspectiveCamera
Camera *-- BaseMovement : moveComponent

%% Scene orchestration roots
Scene *-- Camera : activeCamera
Scene *-- CompositeUniformProvider : frameUniforms
Scene o-- Shader : shaderRegistry
Scene o-- Light : lights[*]
Scene o-- GameObject : gameObjects[*]
Scene o-- Volume : volumes[*]

%% Render path roots
GameObject o-- Mesh : meshes[*]
Mesh o-- Geometry : geometry
Mesh o-- Material : material
Material o-- Shader : shader
Material o-- Texture : diffuseTexture
Material o-- Texture : specularTexture

%% Volume root branch
Volume o-- Shader : shader
Volume o-- Geometry : volumeGeometry
```

### Qt Host

The application is now hosted by a barebones Qt OpenGL window (`QOpenGLWindow`) that drives `Scene::Update` and `Scene::Render` directly.

Entry point: `src/app/main.cpp`
Window/runtime integration: `src/ui/widgets/DTISceneWidget.cpp`

### Uniform Provider System

This diagram focuses on the uniform application contract and composition path used at frame time, showing which scene actors can contribute uniform state and how providers are aggregated.

Source: [docs/uniform-provider-class-diagram.mmd](docs/uniform-provider-class-diagram.mmd)

```mermaid
classDiagram

class UniformProvider {
	<<abstract>>
	+Apply(shader: Shader&) const
}

class CompositeUniformProvider
class TypedUniformProvider
class Shader
class Camera {
	<<abstract>>
}
class PerspectiveCamera
class Light {
	<<abstract>>
}
class GameObject
class Volume {
	<<abstract>>
}

class Scene

UniformProvider <|-- CompositeUniformProvider
UniformProvider <|-- TypedUniformProvider
UniformProvider <|-- Shader
UniformProvider <|-- Camera
UniformProvider <|-- Light
UniformProvider <|-- Scene
UniformProvider <|-- GameObject
UniformProvider <|-- Volume

Camera <|-- PerspectiveCamera

CompositeUniformProvider o-- UniformProvider : providers[*]
Scene *-- CompositeUniformProvider : frameUniforms
Scene *-- Camera : camera
Scene o-- Shader : shaders[*]
Scene o-- Light : lights[*]
Scene o-- GameObject : gameObjects[*]
Scene o-- Volume : volumes[*]
```

### Volume System

This diagram details the volume type hierarchy, rendering contracts, texture-set composition, and how generic volume data metadata and file I/O connect into the runtime volume objects.

Source: [docs/volume-class-diagram.mmd](docs/volume-class-diagram.mmd)

```mermaid
classDiagram

class UniformProvider {
	<<abstract>>
	+Apply(shader: Shader&) const
}

class IDrawable {
	<<interface>>
	+Draw(frameUniforms: UniformProvider) const
}

class IInspectable {
	<<interface>>
	+CollectInspectableFields(out, groupPrefix)
}

class Volume {
	<<abstract>>
	+Apply(shader: Shader&) const
	+Draw(frameUniforms: UniformProvider) const
	+IsValid() const bool
	+CollectInspectableFields(out, groupPrefix)
	+CollectInspectableNodes(out, nodePrefix)
	#GetTextureSet() const VolumeTextureSet&
}

class FloatVolume
class UInt8Volume
class UInt16Volume
class Mat3Volume

class VolumeTextureSet {
	+AddTexture(texture: shared_ptr~Texture3D~)
	+Bind(shader: Shader&, baseUnit: unsigned int, uniformBaseName: string) const
	+IsValid() const bool
	+Size() const size_t
}

class VolumeData~TVoxel~
class VolumeMetadata
class VolumeFileLoader {
	+Load(filePath: string) optional~LoadedVolumeVariant~
	+LoadTyped~TVoxel~(filePath: string) optional~VolumeData~TVoxel~~
	+Save~TVoxel~(filePath: string, volume: VolumeData~TVoxel~) bool
}

class VolumeGeometry
class Shader
class Texture3D

UniformProvider <|-- Volume
IDrawable <|-- Volume
IInspectable <|-- Volume

Volume <|-- FloatVolume
Volume <|-- UInt8Volume
Volume <|-- UInt16Volume
Volume <|-- Mat3Volume

Volume *-- VolumeGeometry : geometry
Volume o-- Shader : shader

FloatVolume *-- VolumeTextureSet : textureSet
UInt8Volume *-- VolumeTextureSet : textureSet
UInt16Volume *-- VolumeTextureSet : textureSet
Mat3Volume *-- VolumeTextureSet : textureSet
VolumeTextureSet o-- Texture3D : many

VolumeData~TVoxel~ *-- VolumeMetadata : metadata
VolumeFileLoader ..> VolumeData~TVoxel~ : load/save
VolumeFileLoader ..> VolumeMetadata : writes header
```

### MRI Preprocessing Pipeline and Runner

This diagram explains the preprocessing flow model: request/context/report/result types, stage-based pipeline execution, preprocessor ownership of the pipeline, and runner orchestration/output persistence.

Source: [docs/preprocessing-pipeline-runner-class-diagram.mmd](docs/preprocessing-pipeline-runner-class-diagram.mmd)

```mermaid
classDiagram

class MriPreprocessingRequest {
	+string datasetRootPath
	+string preferredSubjectId
	+string preferredSessionId
	+bool preferAnatomicalVolumes
	+bool generateAllChannels
}

class MriPreprocessingReport {
	+string sourceVolumePath
	+vector~string~ executedStages
	+vector~string~ warnings
}

class MriPreprocessingResult {
	+DTIVolumeChannels channels
	+MriPreprocessingReport report
}

class MriPreprocessingContext {
	+MriPreprocessingRequest request
	+MriPreprocessingReport report
	+vector~string~ candidateVolumePaths
	+string selectedSourceVolumePath
	+VolumeData~float~ loadedVolume
	+VolumeData~float~ normalizedVolume
	+DTIVolumeChannels outputChannels
}

class IMriPreprocessingStage {
	<<interface>>
	+Name() const char*
	+Execute(context: MriPreprocessingContext&) const
}

class MriPreprocessingPipeline {
	+AddStage(stage: unique_ptr~IMriPreprocessingStage~) MriPreprocessingPipeline&
	+Execute(request: const MriPreprocessingRequest&) const MriPreprocessingResult
	-vector~unique_ptr~IMriPreprocessingStage~~ stages
}

class DatasetDiscoveryStage {
	+Name() const char*
	+Execute(context: MriPreprocessingContext&) const
}

class ScalarVolumeLoadStage {
	+Name() const char*
	+Execute(context: MriPreprocessingContext&) const
}

class IntensityNormalizationStage {
	+Name() const char*
	+Execute(context: MriPreprocessingContext&) const
}

class DerivedDtiChannelSynthesisStage {
	+Name() const char*
	+Execute(context: MriPreprocessingContext&) const
}

class MriToDtiPreprocessor {
	+MriToDtiPreprocessor()
	+Process(request: const MriPreprocessingRequest&) const MriPreprocessingResult
	-MriPreprocessingPipeline pipeline
}

class MriPreprocessingRunnerRequest {
	+MriPreprocessingRequest preprocessingRequest
	+string outputDirectory
	+string outputBasename
}

class MriPreprocessingRunnerResult {
	+bool success
	+string message
	+MriPreprocessingResult preprocessingResult
	+vector~string~ writtenFiles
}

class MriPreprocessingRunner {
	+MriPreprocessingRunner()
	+Run(request: const MriPreprocessingRunnerRequest&) const MriPreprocessingRunnerResult
	+BuildSummary(result: const MriPreprocessingRunnerResult&) static string
}

IMriPreprocessingStage <|-- DatasetDiscoveryStage
IMriPreprocessingStage <|-- ScalarVolumeLoadStage
IMriPreprocessingStage <|-- IntensityNormalizationStage
IMriPreprocessingStage <|-- DerivedDtiChannelSynthesisStage

MriPreprocessingPipeline *-- IMriPreprocessingStage : stages
MriPreprocessingPipeline ..> MriPreprocessingContext : executes
MriPreprocessingPipeline ..> MriPreprocessingRequest : input
MriPreprocessingPipeline ..> MriPreprocessingResult : output

MriPreprocessingContext *-- MriPreprocessingRequest : request
MriPreprocessingContext *-- MriPreprocessingReport : report
MriPreprocessingResult *-- MriPreprocessingReport : report

MriToDtiPreprocessor *-- MriPreprocessingPipeline : owns
MriToDtiPreprocessor ..> MriPreprocessingRequest : Process input
MriToDtiPreprocessor ..> MriPreprocessingResult : Process output

MriPreprocessingRunnerRequest *-- MriPreprocessingRequest : preprocessingRequest
MriPreprocessingRunnerResult *-- MriPreprocessingResult : preprocessingResult
MriPreprocessingRunner ..> MriPreprocessingRunnerRequest : Run input
MriPreprocessingRunner ..> MriPreprocessingRunnerResult : Run output
MriPreprocessingRunner ..> MriToDtiPreprocessor : uses in Run
```

## Sequence Diagrams

### Application Frame Loop (Qt)

The render loop is driven by Qt timer updates in `DTISceneWidget` and no longer relies on GLFW/ImGui frame orchestration.

### Scene Render Sequence

This diagram focuses on `Scene::Render`: frame uniform setup, enabled light indexing, and draw ordering for game objects, skybox, and volumes.

Source: [docs/scene-render-sequence-diagram.mmd](docs/scene-render-sequence-diagram.mmd)

```mermaid
sequenceDiagram
	autonumber
	participant Scene
	participant GL as OpenGL
	participant Frame as CompositeUniformProvider
	participant Cam as Camera
	participant Light as Light[i]
	participant Obj as GameObject[i]
	participant Sky as Skybox
	participant Vol as Volume[i]

	Scene->>GL: glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
	Scene->>Scene: count enabled lights (clamped to max)

	Scene->>Frame: ClearProviders()
	Scene->>Frame: AddProvider(*Scene)
	opt camera exists
		Scene->>Frame: AddProvider(*camera)
		Scene->>Cam: (provider referenced by frame uniforms)
	end

	loop for each enabled light
		Scene->>Light: SetUniformIndex(i)
		Scene->>Frame: AddProvider(*light)
	end

	loop for each game object
		Scene->>Obj: Draw(frameUniforms)
	end

	opt skybox and camera exist
		Scene->>Sky: Draw(*camera)
	end

	loop for each non-null volume
		Scene->>Vol: Draw(frameUniforms)
	end
```

### MRI Preprocessing Runner Sequence

This diagram traces preprocessing execution from runner input validation to stage-by-stage pipeline execution and channel persistence to `.vxa` files.

Source: [docs/preprocessing-runner-sequence-diagram.mmd](docs/preprocessing-runner-sequence-diagram.mmd)

```mermaid
sequenceDiagram
	autonumber
	participant Caller
	participant Runner as MriPreprocessingRunner
	participant FS as std::filesystem
	participant Pre as MriToDtiPreprocessor
	participant Pipe as MriPreprocessingPipeline
	participant S1 as DatasetDiscoveryStage
	participant S2 as ScalarVolumeLoadStage
	participant S3 as IntensityNormalizationStage
	participant S4 as DerivedDtiChannelSynthesisStage
	participant IO as VolumeFileLoader

	Caller->>Runner: Run(request)
	Runner->>Runner: validate datasetRootPath / outputDirectory
	Runner->>FS: create_directories(outputDirectory)
	Runner->>Runner: resolve outputBaseName (default dti_proxy)

	Runner->>Pre: Process(preprocessingRequest)
	Pre->>Pipe: Execute(request)
	Pipe->>Pipe: create MriPreprocessingContext

	Pipe->>S1: Execute(context)
	Pipe->>Pipe: report.executedStages.push_back(Name())
	Pipe->>S2: Execute(context)
	Pipe->>Pipe: report.executedStages.push_back(Name())
	Pipe->>S3: Execute(context)
	Pipe->>Pipe: report.executedStages.push_back(Name())
	Pipe->>S4: Execute(context)
	Pipe->>Pipe: report.executedStages.push_back(Name())
	Pipe-->>Pre: MriPreprocessingResult
	Pre-->>Runner: MriPreprocessingResult

	Runner->>IO: Save(faPath, channels.fa)
	alt md present
		Runner->>IO: Save(mdPath, *channels.md)
	end
	alt ad present
		Runner->>IO: Save(adPath, *channels.ad)
	end
	alt rd present
		Runner->>IO: Save(rdPath, *channels.rd)
	end

	Runner->>Runner: success=true, message=... , writtenFiles+=paths
	Runner-->>Caller: MriPreprocessingRunnerResult
```

## Troubleshooting

- `Cannot open include file` errors after header moves:
	- Ensure include subdirectories are listed in `target_include_directories` in `CMakeLists.txt`.
- Missing package errors from CMake:
	- Re-run vcpkg install command for all required packages.
- Runtime missing DLLs:
	- `debug.ps1` copies vcpkg DLLs from `C:/vcpkg/installed/x64-windows/bin`.
