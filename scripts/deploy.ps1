# Deploy contcal build to SteamVR. Run with SteamVR STOPPED.
param(
    [string]$SteamRoot = "${env:ProgramFiles(x86)}\Steam\steamapps\common"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$release = Join-Path $repoRoot "bin\artifacts\Release"
$driverSrc = Join-Path $repoRoot "bin\driver_01spacecalibrator\bin\win64"
$manifestSrc = Join-Path $repoRoot "driver_01spacecalibrator\driver.vrdrivermanifest"

$overlayDest = Join-Path $SteamRoot "01spacecalibrator"
$driverRoot = Join-Path $SteamRoot "SteamVR\drivers\01spacecalibrator"
$driverDest = Join-Path $driverRoot "bin\win64"

$required = @(
    (Join-Path $release "SpaceCalibrator.exe"),
    (Join-Path $driverSrc "driver_01spacecalibrator.dll"),
    $manifestSrc
)
foreach ($p in $required) {
    if (-not (Test-Path $p)) {
        Write-Error "Missing build artifact: $p`nRun: cmake --build bin --config Release"
    }
}

New-Item -ItemType Directory -Force -Path (Join-Path $overlayDest "bin\win64") | Out-Null
New-Item -ItemType Directory -Force -Path $driverDest | Out-Null

Copy-Item (Join-Path $release "SpaceCalibrator.exe") (Join-Path $overlayDest "SpaceCalibrator.exe") -Force
Copy-Item (Join-Path $driverSrc "driver_01spacecalibrator.dll") (Join-Path $overlayDest "bin\win64\driver_01spacecalibrator.dll") -Force
Copy-Item $manifestSrc (Join-Path $overlayDest "driver.vrdrivermanifest") -Force
Copy-Item (Join-Path $driverSrc "driver_01spacecalibrator.dll") (Join-Path $driverDest "driver_01spacecalibrator.dll") -Force
Copy-Item $manifestSrc (Join-Path $driverRoot "driver.vrdrivermanifest") -Force

& (Join-Path $PSScriptRoot "disable-spaceoverride-driver.ps1") -SteamRoot $SteamRoot

Get-ChildItem $driverDest -Filter "*.dll.stale" -ErrorAction SilentlyContinue | Remove-Item -Force

$ver = Select-String -Path (Join-Path $repoRoot "src\common\Version.h") -Pattern 'SPACECAL_VERSION_STRING "(.+)"' |
    ForEach-Object { $_.Matches.Groups[1].Value }

Write-Host "Deployed $ver"
Write-Host "  Overlay:   $overlayDest"
Write-Host "  Manifest:  $(Join-Path $overlayDest 'driver.vrdrivermanifest')"
Write-Host "  Driver:    $driverDest"
Write-Host "  Manifest:  $(Join-Path $driverRoot 'driver.vrdrivermanifest')"
Write-Host "Restart SteamVR to load."
Write-Host "Then run: .\scripts\validate-install.ps1"