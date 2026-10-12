param(
    [string]$OpenRGBSource = "",
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$pluginDirectory = Join-Path $projectRoot "plugin"

if ([string]::IsNullOrWhiteSpace($OpenRGBSource)) {
    $OpenRGBSource = Join-Path $projectRoot "vendor\openrgb-1.0"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $projectRoot "dist"
}

$qmake = Get-Command qmake.exe -ErrorAction SilentlyContinue
if (-not $qmake) {
    throw "Qt qmake.exe is not available. Use the GitHub Actions workflow instead of installing Qt locally."
}
$nmake = Get-Command nmake.exe -ErrorAction SilentlyContinue
if (-not $nmake) {
    throw "MSVC nmake.exe is not available. Run from an x64 Visual Studio developer prompt, or use GitHub Actions."
}
if (-not (Test-Path (Join-Path $OpenRGBSource "OpenRGBPluginInterface.h"))) {
    throw "OpenRGB source was not found: $OpenRGBSource"
}

Push-Location $pluginDirectory
try {
    & $qmake.Source "openrgb-aula-he.pro" "OPENRGB_SOURCE=$OpenRGBSource" "CONFIG+=release"
    if ($LASTEXITCODE -ne 0) { throw "qmake failed." }
    & $nmake.Source
    if ($LASTEXITCODE -ne 0) { throw "nmake failed." }

    $dll = Get-ChildItem -Path $pluginDirectory -Recurse -File |
        Where-Object { $_.Name -in @("OpenRGB-WIN68.dll", "OpenRGBAulaHE.dll") } |
        Select-Object -First 1
    if (-not $dll) {
        Write-Host "Build tree:"; Get-ChildItem -Path $pluginDirectory -Recurse -File | Select-Object FullName
        throw "Build completed but the OpenRGB-WIN68 plugin DLL was not produced."
    }
    $hid = Join-Path $OpenRGBSource "dependencies\hidapi-hotplug-win\x64\hidapi-hotplug.dll"
    if (-not (Test-Path -LiteralPath $hid)) { throw "Matching hidapi-hotplug.dll was not found: $hid" }
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $OutputDirectory "OpenRGB-WIN68.dll") -Force
    Copy-Item -LiteralPath $hid -Destination (Join-Path $OutputDirectory "hidapi-hotplug.dll") -Force
    Write-Host "Built plugin: $(Join-Path $OutputDirectory 'OpenRGB-WIN68.dll')"
    Write-Host "Staged runtime: $(Join-Path $OutputDirectory 'hidapi-hotplug.dll')"
}
finally {
    Pop-Location
}
