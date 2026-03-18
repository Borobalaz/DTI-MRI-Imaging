# OpenGLBase

Base for future OpenGL projects.

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
.\vcpkg.exe install glm:x64-windows glfw3:x64-windows glad:x64-windows assimp:x64-windows stb:x64-windows imgui[docking-experimental,glfw-binding,opengl3-binding]:x64-windows
```

### 3. Toolchain and Dependency Notes

`CMakeLists.txt` requires these packages:

- `OpenGL`
- `glm`
- `glfw3`
- `glad`
- `assimp`
- `imgui` (with `docking-experimental`, `glfw-binding`, and `opengl3-binding` features)
- `stb_image.h` (provided by vcpkg package `stb`)

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

## Output

Expected executable path:

```text
build/Release/app.exe
```

## Troubleshooting

- `Cannot open include file` errors after header moves:
	- Ensure include subdirectories are listed in `target_include_directories` in `CMakeLists.txt`.
- Missing package errors from CMake:
	- Re-run vcpkg install command for all required packages.
- Runtime missing DLLs:
	- `debug.ps1` copies vcpkg DLLs from `C:/vcpkg/installed/x64-windows/bin`.
