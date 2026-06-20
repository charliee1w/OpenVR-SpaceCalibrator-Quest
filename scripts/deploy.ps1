# Deploy contcal build to SteamVR. Run with SteamVR STOPPED.
param(
    [string]$SteamRoot = "${env:ProgramFiles(x86)}\Steam\steamapps\common"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$release = Join-Path $repoRoot "bin\artifacts\Release"
$driverSrc = Join-Path $repoRoot "bin\driver_01spacecalibrator\bin\win64"

$overlayDest = Join-Path $SteamRoot "01spacecalibrator"
$driverDest = Join-Path $SteamRoot "SteamVR\drivers\01spacecalibrator\bin\win64"

foreach ($p in @(
    (Join-Path $release "SpaceCalibrator.exe"),
    (Join-Path $driverSrc "driver_01spacecalibrator.dll")
)) {
    if (-not (Test-Path $p)) {
        Write-Error "Missing build artifact: $p`nRun: cmake --build bin --config Release"
    }
}

New-Item -ItemType Directory -Force -Path (Join-Path $overlayDest "bin\win64") | Out-Null
New-Item -ItemType Directory -Force -Path $driverDest | Out-Null

Copy-Item (Join-Path $release "SpaceCalibrator.exe") (Join-Path $overlayDest "SpaceCalibrator.exe") -Force
Copy-Item (Join-Path $driverSrc "driver_01spacecalibrator.dll") (Join-Path $overlayDest "bin\win64\driver_01spacecalibrator.dll") -Force
Copy-Item (Join-Path $driverSrc "driver_01spacecalibrator.dll") (Join-Path $driverDest "driver_01spacecalibrator.dll") -Force

Get-ChildItem $driverDest -Filter "*.dll.stale" -ErrorAction SilentlyContinue | Remove-Item -Force

$ver = Select-String -Path (Join-Path $repoRoot "src\common\Version.h") -Pattern 'SPACECAL_VERSION_STRING "(.+)"' |
    ForEach-Object { $_.Matches.Groups[1].Value }

Write-Host "Deployed $ver"
Write-Host "  Overlay: $overlayDest"
Write-Host "  Driver:  $driverDest"
Write-Host "Restart SteamVR to load."