# OpenGLBase
Author: Boroczky Balazs

## Developer Environment Setup

This project is a C++17 Qt 6 Widgets + OpenGL application built with CMake and dependencies managed through vcpkg.

The runtime is split into two targets:

- `engine`: a Qt-independent shared library containing the rendering, scene, geometry, texture, volume, and input engine.
- `app_qt`: the Qt Widgets executable containing the application shell, viewport, inspector, and other UI code.

The UI links against `engine`, and both `engine.dll` and `app_qt.exe` are emitted into the same configuration directory so the executable can load the engine at runtime.

The source tree is organized by ownership:

```text
engine/
	include/engine/   Engine public headers
	src/              Engine implementation
	assets/           Engine models, textures, and volumes
	shaders/          Engine shader sources
ui/
	include/ui/       Qt UI headers and widgets
	src/app/          Application entry point
	src/              Qt UI implementation
```

### 1. Prerequisites

- Windows with PowerShell 5.1 or PowerShell 7
- Visual Studio 2022 or a compatible MSVC toolset with the **Desktop development with C++** workload
- CMake 3.20 or newer
- Git
- vcpkg with the `x64-windows` triplet
- Qt 6 built for MSVC 2022 64-bit, including `Core`, `Gui`, `Widgets`, `OpenGL`, and `OpenGLWidgets`
- A working OpenGL driver

The build scripts are PowerShell scripts and are intended to be run from the repository root. The checked-in paths in `settings.json` describe the current development machine; update them when setting up the project on another machine.

You can verify tools from a terminal:

```powershell
cmake --version
git --version
Get-Command cl
```

### 2. Install and Bootstrap vcpkg

Install vcpkg.

```powershell
git clone https://github.com/microsoft/vcpkg.git D:/DevTools/vcpkg
cd D:/DevTools/vcpkg
.\bootstrap-vcpkg.bat
```

Install the required libraries for the configured `x64-windows` triplet:

```powershell
.\vcpkg.exe install glm:x64-windows glad:x64-windows assimp:x64-windows stb:x64-windows
```

Install Qt 6 (MSVC 2022 64-bit) through the Qt Online Installer. 
The scripts use `QT_ROOT` first, then the Qt root and candidate paths in `settings.json`.

### 3. Toolchain and Dependency Notes

`build.ps1` loads `settings.json` and passes the configured vcpkg toolchain and Qt CMake directories to CMake. At minimum, check these values before building:

- `toolchain.cmakeToolchainFile`
- `toolchain.vcpkgRoot`
- `toolchain.vcpkgTriplet`
- `qt.root` and `qt.candidates`
- `project.buildDirectory` and `project.defaultConfiguration`

`CMakeLists.txt` requires these dependencies:

- `OpenGL`
- `glm`
- `glad`
- `assimp`
- `Qt6` (`Core`, `Gui`, `Widgets`, `OpenGL`, `OpenGLWidgets`)
- `stb_image.h` (provided by vcpkg package `stb`)

The normal build command supplies these equivalent CMake options from `settings.json`:

```text
-DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
-DCMAKE_PREFIX_PATH=<qt-root>/lib/cmake
-DQt6_DIR=<qt-root>/lib/cmake/Qt6
```

If you configure CMake manually, provide the same toolchain and Qt paths, or set `VCPKG_ROOT` so the CMake fallback can locate the vcpkg toolchain.

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

Run only (after building):

```powershell
./debug.ps1 Release
```

`debug.ps1` also runs `windeployqt` (when found via `QT_ROOT` or known Qt install paths) so the Qt runtime plugins are copied next to `app_qt.exe`.
The same directory contains `engine.dll`; `debug.ps1` verifies it before launching the UI.

## Manual CMake Build (Alternative)

If you prefer plain CMake commands:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -DCMAKE_PREFIX_PATH="$env:QT_ROOT/lib/cmake" -DQt6_DIR="$env:QT_ROOT/lib/cmake/Qt6"
cmake --build build --config Release
```

## Output

Expected executable path:

```text
build/Release/app_qt.exe
build/Release/engine.dll
```

## Qt Application Structure

- Entry point: `ui/src/app/main.cpp` creates `QApplication`, configures OpenGL 3.3 core profile, and shows `WidgetsMainWindow`.
- Host window: `ui/src/windows/WidgetsMainWindow.cpp` owns the Qt widget layout (viewport, object list, inspector, render stats).
- OpenGL viewport: `ui/src/widgets/OpenGLViewportWidget.cpp` derives from `QOpenGLWidget`, drives repaint via `QTimer`, and runs `Scene::Update` + `Scene::Render` inside `paintGL()`.
- DTI specialization: `ui/src/widgets/DTIViewportWidget.cpp` builds `DtiVolumeScene` and loads DWI/bval/bvec dataset inputs.
