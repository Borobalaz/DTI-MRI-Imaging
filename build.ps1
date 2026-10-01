# Build script for Connectomics-Imaging project (Qt host)
# Usage: .\build.ps1 [Debug|Release]

param(
    [string]$Config
)

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$Settings = Get-Content (Join-Path $ProjectRoot "settings.json") -Raw | ConvertFrom-Json
$Config = if ($Config) { $Config } else { $Settings.project.defaultConfiguration }
$Target = $Settings.project.target
$BuildDirectory = Join-Path $ProjectRoot $Settings.project.buildDirectory


if ($Settings.qt.bypassQtLicenseCheck) {
    $env:QTFRAMEWORK_BYPASS_LICENSE_CHECK = "1"
}

$QtRoot = $Settings.qt.root
if ($env:QT_ROOT -and (Test-Path $env:QT_ROOT)) {
    $QtRoot = $env:QT_ROOT
} else {
    foreach ($Candidate in $Settings.qt.candidates) {
        if (Test-Path (Join-Path $Candidate "lib/cmake/Qt6/Qt6Config.cmake")) {
            $QtRoot = $Candidate
            break
        }
    }
}

if (-not $QtRoot) {
    Write-Host "Qt6 was not found." -ForegroundColor Red
    Write-Host "Set QT_ROOT, e.g.:`n  `$env:QT_ROOT=\"C:/Qt/6.11.0/msvc2022_64\"" -ForegroundColor Yellow
    exit 1
}

Write-Host "=== Configuring CMake ===" -ForegroundColor Green
$CacheFile = Join-Path $BuildDirectory "CMakeCache.txt"
$CachedSourceLine = if (Test-Path $CacheFile) {
    Select-String -Path $CacheFile -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=' | Select-Object -First 1
}
$ExpectedSource = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
$CachedSource = if ($CachedSourceLine) {
    $CachedSourceLine.Line.Substring($CachedSourceLine.Line.IndexOf('=') + 1).Replace('/', '\').TrimEnd('\')
}
$RefreshCache = $CachedSource -and ($CachedSource -ine $ExpectedSource)

$cmakeArgs = @(
    "-S", $ProjectRoot,
    "-B", $BuildDirectory,
    "-DCMAKE_TOOLCHAIN_FILE=$($Settings.toolchain.cmakeToolchainFile)",
    "-DCMAKE_BUILD_TYPE=$Config"
)

$QtCmakeDir = (Join-Path $QtRoot $Settings.qt.cmakeDirectory) -replace "\\", "/"
$Qt6Dir = (Join-Path $QtRoot $Settings.qt.configDirectory) -replace "\\", "/"
$cmakeArgs += "-DCMAKE_PREFIX_PATH=$QtCmakeDir"
$cmakeArgs += "-DQt6_DIR=$Qt6Dir"
Write-Host "Using Qt from: $QtRoot" -ForegroundColor Cyan
if ($RefreshCache) {
    Write-Host "Refreshing CMake cache from a different source directory: $CachedSource" -ForegroundColor Yellow
    $cmakeArgs = @("--fresh") + $cmakeArgs
}

& cmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host "`n=== Building the project ===" -ForegroundColor Green
cmake --build $BuildDirectory --config $Config --target $Target

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "`n=== Build completed successfully ===" -ForegroundColor Green
if (-not (Test-Path (Join-Path $BuildDirectory "$Config\$($Settings.project.engineLibrary)"))) {
    Write-Host "Engine DLL was not produced: $(Join-Path $BuildDirectory "$Config\$($Settings.project.engineLibrary)")" -ForegroundColor Red
    exit 1
}
Write-Host "Executable: $(Join-Path $BuildDirectory "$Config\$($Settings.project.executable)")" -ForegroundColor Cyan
Write-Host "Engine DLL: $(Join-Path $BuildDirectory "$Config\$($Settings.project.engineLibrary)")" -ForegroundColor Cyan
