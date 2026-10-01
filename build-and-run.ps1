# Build and run script (Qt host convenience wrapper)
# Usage: .\build-and-run.ps1 [Debug|Release]

param(
    [string]$Config
)

$ErrorActionPreference = "Stop"

$Settings = Get-Content (Join-Path $PSScriptRoot "settings.json") -Raw | ConvertFrom-Json
$Config = if ($Config) { $Config } else { $Settings.project.defaultConfiguration }

if ($Settings.qt.bypassQtLicenseCheck) {
    $env:QTFRAMEWORK_BYPASS_LICENSE_CHECK = "1"
}

Write-Host "=== Building and Running ===" -ForegroundColor Cyan

.\build.ps1 $Config
if ($LASTEXITCODE -ne 0) { exit 1 }

.\debug.ps1 $Config
