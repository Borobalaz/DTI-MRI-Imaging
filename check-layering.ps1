# Checks the engine's layered include graph (see .claude/CLAUDE.md, "Engine on-disk layout and layering").
# Usage: .\check-layering.ps1   (build.ps1 runs it before configuring CMake)
#
# Every #include in engine/, core/ and ui/ .h/.cpp files is checked:
#   - engine layers may only include layers below them:
#       public <- foundation <- render <- scene <- runtime; engine/src/public/Engine.cpp may include anything
#   - core/ may include public, foundation, render, scene (only runtime includes core)
#   - headers under <layer>/internal/ may only be included from the same layer
#   - glad may only be included from engine/src/render/ (and the render/internal/ headers that wrap it)
#   - ui/ may include only its own ui/include headers and the engine's public headers
# Violations are printed as "file:line: message"; the script exits 1 if there are any.

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$EngineInclude = Join-Path $ProjectRoot "engine/include"
$PublicInclude = Join-Path $EngineInclude "public"
$CoreInclude = Join-Path $ProjectRoot "core/include"
$UiInclude = Join-Path $ProjectRoot "ui/include"

$EngineLayers = @("foundation", "render", "scene", "runtime")

# Which include classes each source class may depend on. "facade" (Engine.cpp) is unrestricted.
$AllowedIncludes = @{
    "public"     = @("public")
    "foundation" = @("public", "foundation")
    "render"     = @("public", "foundation", "render")
    "scene"      = @("public", "foundation", "render", "scene")
    "runtime"    = @("public", "foundation", "render", "scene", "runtime", "core")
    "core"       = @("public", "foundation", "render", "scene", "core")
}

$IncludePattern = '^\s*#\s*include\s*([<"])([^>"]+)[>"]'

function Get-RelativePath([string]$Path) {
    return $Path.Substring($ProjectRoot.Length).TrimStart('\', '/').Replace('\', '/')
}

# Classifies a source file by its repo-relative path.
function Get-SourceClass([string]$RelativePath) {
    if ($RelativePath -like "engine/include/public/*") { return "public" }
    if ($RelativePath -like "engine/src/public/*") { return "facade" }
    foreach ($Layer in $EngineLayers) {
        if ($RelativePath -like "engine/include/$Layer/*" -or $RelativePath -like "engine/src/$Layer/*") { return $Layer }
    }
    if ($RelativePath -like "core/*") { return "core" }
    if ($RelativePath -like "ui/*") { return "ui" }
    return $null
}

# Classifies a quoted engine/core include by its prefix (falling back to where it resolves).
function Get-IncludeClass([string]$Include, [string]$IncluderDirectory) {
    $FirstSegment = $Include.Split('/')[0]
    if ($EngineLayers -contains $FirstSegment) { return $FirstSegment }
    if (Test-Path -LiteralPath (Join-Path $PublicInclude $Include) -PathType Leaf) { return "public" }
    if (Test-Path -LiteralPath (Join-Path $CoreInclude $Include) -PathType Leaf) { return "core" }
    $Relative = Join-Path $IncluderDirectory $Include
    if (Test-Path -LiteralPath $Relative -PathType Leaf) {
        return Get-SourceClass (Get-RelativePath ([System.IO.Path]::GetFullPath($Relative)))
    }
    return $null
}

function Test-UiInclude([string]$Include, [string]$IncluderDirectory) {
    foreach ($Root in @($IncluderDirectory, $UiInclude, $PublicInclude)) {
        if (Test-Path -LiteralPath (Join-Path $Root $Include) -PathType Leaf) { return $true }
    }
    return $false
}

$Violations = New-Object System.Collections.Generic.List[string]
$SourceRoots = @("engine/include", "engine/src", "core", "ui") | ForEach-Object { Join-Path $ProjectRoot $_ }
$Files = Get-ChildItem -Path $SourceRoots -Recurse -File -Include *.h, *.cpp

foreach ($File in $Files) {
    $RelativePath = Get-RelativePath $File.FullName
    $SourceClass = Get-SourceClass $RelativePath
    if (-not $SourceClass) { continue }
    $IsGladAllowed = ($RelativePath -like "engine/src/render/*") -or ($RelativePath -like "engine/include/render/internal/*")

    $Lines = [System.IO.File]::ReadAllLines($File.FullName)
    for ($Index = 0; $Index -lt $Lines.Length; $Index++) {
        $Match = [regex]::Match($Lines[$Index], $IncludePattern)
        if (-not $Match.Success) { continue }
        $Delimiter = $Match.Groups[1].Value
        $Include = $Match.Groups[2].Value
        $Location = "${RelativePath}:$($Index + 1)"

        if ($Include -like "glad/*") {
            if (-not $IsGladAllowed) {
                $Violations.Add("${Location}: glad may only be included under engine/src/render/ (or engine/include/render/internal/): $Include")
            }
            continue
        }
        if ($Delimiter -ne '"') { continue }

        if ($SourceClass -eq "ui") {
            if (-not (Test-UiInclude $Include $File.DirectoryName)) {
                $Violations.Add("${Location}: ui/ may include only ui/include headers and engine public headers (engine/include/public): `"$Include`"")
            }
            continue
        }

        $IncludeClass = Get-IncludeClass $Include $File.DirectoryName
        if (-not $IncludeClass) {
            $Violations.Add("${Location}: include does not resolve to a known layer: `"$Include`"")
            continue
        }

        $IsInternal = $Include -match '(^|/)internal/'
        if ($IsInternal -and $IncludeClass -ne $SourceClass) {
            $Violations.Add("${Location}: '$IncludeClass' internal header included from layer '$SourceClass': `"$Include`"")
            continue
        }

        if ($SourceClass -eq "facade") { continue }
        if ($AllowedIncludes[$SourceClass] -notcontains $IncludeClass) {
            $Violations.Add("${Location}: layer '$SourceClass' may not include layer '$IncludeClass': `"$Include`"")
        }
    }
}

if ($Violations.Count -gt 0) {
    foreach ($Violation in $Violations) { Write-Host $Violation -ForegroundColor Red }
    Write-Host "Layering check failed: $($Violations.Count) violation(s)." -ForegroundColor Red
    exit 1
}

Write-Host "Layering check passed ($($Files.Count) files)." -ForegroundColor Green
exit 0
