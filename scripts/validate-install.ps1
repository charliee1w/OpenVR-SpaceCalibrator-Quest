# Verify SpaceCal overlay + driver install under Steam.
param(
    [string]$SteamRoot = "${env:ProgramFiles(x86)}\Steam\steamapps\common",
    [switch]$RequireSteamVR
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent

$expectedVer = Select-String -Path (Join-Path $repoRoot "src\common\Version.h") -Pattern 'SPACECAL_VERSION_STRING "(.+)"' |
    ForEach-Object { $_.Matches.Groups[1].Value }

$overlayExe = Join-Path $SteamRoot "01spacecalibrator\SpaceCalibrator.exe"
$overlayDll = Join-Path $SteamRoot "01spacecalibrator\bin\win64\driver_01spacecalibrator.dll"
$overlayManifest = Join-Path $SteamRoot "01spacecalibrator\driver.vrdrivermanifest"
$driverDll = Join-Path $SteamRoot "SteamVR\drivers\01spacecalibrator\bin\win64\driver_01spacecalibrator.dll"
$driverManifest = Join-Path $SteamRoot "SteamVR\drivers\01spacecalibrator\driver.vrdrivermanifest"
$driverLog = Join-Path $SteamRoot "SteamVR\drivers\01spacecalibrator\bin\win64\space_calibrator_driver.log"
$ipcPipe = "\\.\pipe\OpenVRSpaceCalibratorDriver"

$checks = [System.Collections.Generic.List[object]]::new()

function Add-Check([string]$Name, [bool]$Pass, [string]$Detail) {
    $checks.Add([pscustomobject]@{ Name = $Name; Pass = $Pass; Detail = $Detail })
}

Add-Check "overlay exe" (Test-Path $overlayExe) $overlayExe
Add-Check "overlay driver dll" (Test-Path $overlayDll) $overlayDll
Add-Check "overlay manifest" (Test-Path $overlayManifest) $overlayManifest
Add-Check "steamvr driver dll" (Test-Path $driverDll) $driverDll
Add-Check "steamvr manifest" (Test-Path $driverManifest) $driverManifest

$stale = Get-ChildItem (Split-Path $driverDll) -Filter "*.dll.stale" -ErrorAction SilentlyContinue
Add-Check "no stale driver dll" ($stale.Count -eq 0) $(if ($stale.Count -gt 0) { $stale.FullName -join '; ' } else { "ok" })

if ((Test-Path $overlayDll) -and (Test-Path $driverDll)) {
    $overlayHash = (Get-FileHash $overlayDll -Algorithm SHA256).Hash
    $driverHash = (Get-FileHash $driverDll -Algorithm SHA256).Hash
    Add-Check "overlay/driver dll match" ($overlayHash -eq $driverHash) `
        $(if ($overlayHash -eq $driverHash) { "sha256 match" } else { "mismatch - redeploy with deploy.ps1" })
}

$loadedVer = $null
if (Test-Path $driverLog) {
    $loadedVer = Select-String -Path $driverLog -Pattern 'OpenVR-SpaceCalibratorDriver\s+([\d.]+-gore-contcal\d+)\s+loaded' |
        Select-Object -Last 1 | ForEach-Object { $_.Matches.Groups[1].Value }
}
if ($loadedVer) {
    Add-Check "driver log version" ($loadedVer -eq $expectedVer) "log=$loadedVer expected=$expectedVer"
} else {
    Add-Check "driver log version" $false "no load line in $driverLog (start SteamVR after deploy)"
}

$ipcUp = Test-Path $ipcPipe
if ($RequireSteamVR) {
    Add-Check "ipc pipe" $ipcUp $ipcPipe
} elseif ($ipcUp) {
    Add-Check "ipc pipe (optional)" $true $ipcPipe
}

Write-Host "SpaceCal install validation - expected $expectedVer" -ForegroundColor Cyan
$fail = 0
foreach ($c in $checks) {
    $color = if ($c.Pass) { "Green" } else { "Red" }
    $mark = if ($c.Pass) { "PASS" } else { "FAIL"; $fail++ }
    Write-Host ("  [{0}] {1,-24} {2}" -f $mark, $c.Name, $c.Detail) -ForegroundColor $color
}

if ($fail -gt 0) {
    Write-Host "`n$fail check(s) failed. Run .\scripts\deploy.ps1 with SteamVR stopped." -ForegroundColor Red
    exit 1
}

Write-Host "`nAll checks passed." -ForegroundColor Green
exit 0