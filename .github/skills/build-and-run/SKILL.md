---
name: build-and-run
description: Build, deploy, and run the Windows Qt application using the project's PowerShell scripts.
---

# Build and run the application

Use this skill when building, launching, or cleaning the project. **Use the repository's PowerShell scripts; do not replace them with direct CMake commands or manual executable launches.** The scripts configure the vcpkg and Qt paths, build the configured target, stage runtime dependencies, and launch from the expected working directory.

Run commands from the repository root in PowerShell:

- Build with the default configuration from `settings.json`: `.\build.ps1`
- Build Debug or Release: `.\build.ps1 Debug` or `.\build.ps1 Release`
- Build and then launch in one step: `.\build-and-run.ps1`
- Launch an already-built configuration: `.\debug.ps1 Release` (or `Debug`)
- Remove build artifacts when a clean build is needed: `.\clean.ps1`

`build-and-run.ps1` uses the configured default when no configuration is supplied; pass `Debug` or `Release` to select one. Run `debug.ps1` only after that configuration has been built. The scripts read `settings.json`; if dependencies are installed in different locations, update the relevant local Qt/vcpkg settings or set `QT_ROOT` to the Qt installation before running them. Keep the compiler, Qt, vcpkg, and triplet configuration consistent with the installed toolchain.

The build target and output names are configured in `settings.json`. By default, the executable and engine library are produced under `build\<Configuration>\` as `app_qt.exe` and `engine.dll`. The run script stages runtime DLLs and deploys Qt when the configured tools are available.

If a script exits with an error, report the failing script and its output; do not silently fall back to a manual build or launch. `clean.ps1` removes the repository's `build` directory, so use it only when cleaning build artifacts is intended.
