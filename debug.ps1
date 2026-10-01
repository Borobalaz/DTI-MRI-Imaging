# Debug/Run script for Connectomics-Imaging Qt host
# Usage: .\debug.ps1 [Debug|Release]

param(
    [string]$Config
)

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$Settings = Get-Content (Join-Path $ProjectRoot "settings.json") -Raw | ConvertFrom-Json
$Config = if ($Config) { $Config } else { $Settings.project.defaultConfiguration }
$TargetName = $Settings.project.executable
$BuildDir = Join-Path $ProjectRoot "$($Settings.project.buildDirectory)\$Config"

if ($Settings.qt.bypassQtLicenseCheck) {
    $env:QTFRAMEWORK_BYPASS_LICENSE_CHECK = "1"
}

if (-not (Test-Path $BuildDir)) {
    Write-Host "Build directory not found: $BuildDir" -ForegroundColor Red
    Write-Host "Build the project first with .\build.ps1 $Config" -ForegroundColor Yellow
    exit 1
}

$BuildDirPath = (Resolve-Path $BuildDir).Path
$ExePath = Join-Path $BuildDirPath $TargetName
$EngineDllPath = Join-Path $BuildDirPath "engine.dll"

if (-not (Test-Path $ExePath)) {
    Write-Host "Executable not found: $ExePath" -ForegroundColor Red
    Write-Host "Build the project first with .\build.ps1 $Config" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $EngineDllPath)) {
    Write-Host "Engine DLL not found: $EngineDllPath" -ForegroundColor Red
    Write-Host "Build the project first with .\build.ps1 $Config" -ForegroundColor Yellow
    exit 1
}

Write-Host "Engine runtime: $EngineDllPath" -ForegroundColor Cyan

Write-Host "=== Copying vcpkg dependencies ===" -ForegroundColor Green

$VcpkgBinPath = $Settings.toolchain.vcpkgBinaryDirectory

if (Test-Path $VcpkgBinPath) {
    Copy-Item "$VcpkgBinPath\*.dll" -Destination $BuildDir -Force -ErrorAction SilentlyContinue
    Write-Host "DLLs copied to build directory" -ForegroundColor Cyan
} else {
    Write-Host "Warning: vcpkg bin directory not found at $VcpkgBinPath" -ForegroundColor Yellow
}

Write-Host "`n=== Deploying Qt runtime ===" -ForegroundColor Green

$QtRoot = $Settings.qt.root
if ($env:QT_ROOT -and (Test-Path $env:QT_ROOT)) {
    $QtRoot = $env:QT_ROOT
} else {
    foreach ($Candidate in $Settings.qt.candidates) {
        if (Test-Path (Join-Path $Candidate $Settings.qt.deployTool)) {
            $QtRoot = $Candidate
            break
        }
    }
}

if (-not $QtRoot) {
    Write-Host "Warning: Could not locate Qt runtime root. Set QT_ROOT to enable automatic Qt deployment." -ForegroundColor Yellow
} else {
    $DeployTool = Join-Path $QtRoot $Settings.qt.deployTool
    if (Test-Path $DeployTool) {
        $DeployArguments = @("--dir", $BuildDir, $ExePath)
        if (-not $Settings.cmake.deployQtTranslations) { $DeployArguments += "--no-translations" }
        if (-not $Settings.cmake.deployQtOpenGLSoftwareRendering) { $DeployArguments += "--no-opengl-sw" }
        & $DeployTool @DeployArguments
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Warning: windeployqt failed (code: $LASTEXITCODE)." -ForegroundColor Yellow
        } else {
            Write-Host "Qt runtime deployed to $BuildDir" -ForegroundColor Cyan
        }
    } else {
        Write-Host "Warning: windeployqt not found at $DeployTool" -ForegroundColor Yellow
    }
}

Write-Host "`n=== Running application ===" -ForegroundColor Green
Push-Location $BuildDirPath
try {
    & $ExePath
    $ApplicationExitCode = $LASTEXITCODE
}
finally {
    Pop-Location
}

if ($ApplicationExitCode -ne 0) {
    Write-Host "`nApplication exited with code: $ApplicationExitCode" -ForegroundColor Yellow
    exit $ApplicationExitCode
}
