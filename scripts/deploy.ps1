# Deploy contcal build to SteamVR. Run with SteamVR STOPPED.
param(
    [string]$SteamRoot = "${env:ProgramFiles(x86)}\Steam\steamapps\common"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent

function Remove-StaleDriverArtifacts {
    param([Parameter(Mandatory = $true)][string]$DriverDir)

    foreach ($pattern in @('*.dll.stale', '*.dll.new')) {
        Get-ChildItem $DriverDir -Filter $pattern -ErrorAction SilentlyContinue | ForEach-Object {
            try {
                Remove-Item $_.FullName -Force -ErrorAction Stop
                Write-Host "Removed leftover driver artifact: $($_.Name)"
            } catch {
                Write-Warning "Could not remove $($_.FullName): $($_.Exception.Message)"
            }
        }
    }
}

function Copy-DriverDll {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )
    try {
        Copy-Item $Source $Destination -Force
        return
    } catch {
        # Steam.exe often keeps the driver DLL mapped even when SteamVR is stopped.
        $staging = "$Destination.new"
        $stale = "$Destination.stale"
        Copy-Item $Source $staging -Force
        if (Test-Path $Destination) {
            Rename-Item $Destination $stale -Force
        }
        Move-Item $staging $Destination -Force
        Remove-StaleDriverArtifacts (Split-Path $Destination -Parent)
        Write-Host "Driver DLL updated via staging rename (Steam had file locked)"
    }
}
function Get-BuildRoot([string]$Root) {
    $bestRoot = $null
    $bestTime = [datetime]::MinValue
    foreach ($name in @("build", "bin")) {
        $candidate = Join-Path $Root $name
        $exe = Join-Path $candidate "artifacts\Release\SpaceCalibrator.exe"
        if (-not (Test-Path $exe)) { continue }
        $written = (Get-Item $exe).LastWriteTime
        if ($written -gt $bestTime) {
            $bestTime = $written
            $bestRoot = $candidate
        }
    }
    if (-not $bestRoot) {
        Write-Error "No Release build found under build/ or bin/. Run: cmake --build build --config Release"
    }
    return $bestRoot
}

$buildRoot = Get-BuildRoot $repoRoot
$release = Join-Path $buildRoot "artifacts\Release"
$driverSrc = Join-Path $buildRoot "driver_01spacecalibrator\bin\win64"
$manifestSrc = Join-Path $repoRoot "driver_01spacecalibrator\driver.vrdrivermanifest"
Write-Host "Using build root: $buildRoot"

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
        Write-Error "Missing build artifact: $p`nRun: cmake --build build --config Release"
    }
}

& (Join-Path $PSScriptRoot "validate-cal-invariants.ps1")

Write-Host "Stopping SteamVR / overlay processes before driver deploy..."
& (Join-Path $PSScriptRoot "kill-vr-ghosts.ps1")
Start-Sleep -Seconds 2
Remove-StaleDriverArtifacts $driverDest
Remove-StaleDriverArtifacts (Join-Path $overlayDest "bin\win64")

New-Item -ItemType Directory -Force -Path (Join-Path $overlayDest "bin\win64") | Out-Null
New-Item -ItemType Directory -Force -Path $driverDest | Out-Null

Copy-Item (Join-Path $release "SpaceCalibrator.exe") (Join-Path $overlayDest "SpaceCalibrator.exe") -Force
Copy-DriverDll (Join-Path $driverSrc "driver_01spacecalibrator.dll") (Join-Path $overlayDest "bin\win64\driver_01spacecalibrator.dll")
Copy-Item $manifestSrc (Join-Path $overlayDest "driver.vrdrivermanifest") -Force
Copy-DriverDll (Join-Path $driverSrc "driver_01spacecalibrator.dll") (Join-Path $driverDest "driver_01spacecalibrator.dll")
Copy-Item $manifestSrc (Join-Path $driverRoot "driver.vrdrivermanifest") -Force

& (Join-Path $PSScriptRoot "disable-spaceoverride-driver.ps1") -SteamRoot $SteamRoot

Remove-StaleDriverArtifacts $driverDest
Remove-StaleDriverArtifacts (Join-Path $overlayDest "bin\win64")

$ver = Select-String -Path (Join-Path $repoRoot "src\common\Version.h") -Pattern 'SPACECAL_VERSION_STRING "(.+)"' |
    ForEach-Object { $_.Matches.Groups[1].Value }

$deployedExe = Join-Path $overlayDest "SpaceCalibrator.exe"
if (Select-String -Path $deployedExe -Pattern "Conflicting Space Calibrator install" -Quiet) {
    Write-Error "Deployed overlay still contains stale multi-install popup strings. Rebuild Release and redeploy."
}

Write-Host "Deployed $ver"
Write-Host "  Overlay:   $overlayDest"
Write-Host "  Manifest:  $(Join-Path $overlayDest 'driver.vrdrivermanifest')"
Write-Host "  Driver:    $driverDest"
Write-Host "  Manifest:  $(Join-Path $driverRoot 'driver.vrdrivermanifest')"
Write-Host "Restart SteamVR to load."
Write-Host "Then run: .\scripts\validate-install.ps1"