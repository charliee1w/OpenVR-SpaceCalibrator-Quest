# contcal5 pose-contract invariants. Fails if post-contcal45 regressions creep back in.
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent

$checks = [System.Collections.Generic.List[object]]::new()

function Add-Check([string]$Name, [bool]$Pass, [string]$Detail) {
    $checks.Add([pscustomobject]@{ Name = $Name; Pass = $Pass; Detail = $Detail })
}

$calCpp = Join-Path $repoRoot "src\overlay\Calibration.cpp"
$driverCpp = Join-Path $repoRoot "src\driver\ServerTrackedDeviceProvider.cpp"
$calPoses = Join-Path $repoRoot "src\overlay\CalibrationPoses.cpp"

$calCppText = Get-Content $calCpp -Raw
$driverText = Get-Content $driverCpp -Raw

Add-Check "no CalibrationPoses split module" (-not (Test-Path $calPoses)) `
    "Pose sourcing must stay in Calibration.cpp (unified shmem), not CalibrationPoses.cpp"

Add-Check "no RefreshDevicePosesForCalibration" `
    (-not ($calCppText -match 'RefreshDevicePosesForCalibration')) `
    "Later pose-source split must not return"

Add-Check "shmem read in CalibrationTick" `
    ($calCppText -match 'shmem\.ReadNewPoses') `
    "CalibrationTick must read driver shmem directly"

Add-Check "no VRSystem pose fallback in cal tick" `
    (-not ($calCppText -match 'GetDeviceToAbsoluteTrackingPose')) `
    "Ref/target must not mix VRSystem with hook shmem"

Add-Check "no passive pose throttle in driver" `
    (-not ($driverText -match 'ShouldStreamPassivePose|kPassivePoseStreamIntervalSec')) `
    "Driver must stream all hook poses every frame"

Add-Check "shmem before transform in driver" `
    ($driverText -match 'shmem\.SetPose' -and $driverText -match 'tf\.enabled') `
    "Raw pre-transform poses must be written to shmem first"

Add-Check "relative pose reset on end continuous" `
    ($calCppText -match 'EndContinuousCalibration[\s\S]*relativePosCalibrated = false') `
    "EndContinuousCalibration must clear relativePosCalibrated"

$failed = @($checks | Where-Object { -not $_.Pass })
foreach ($c in $checks) {
    $status = if ($c.Pass) { "PASS" } else { "FAIL" }
    Write-Host ("  [{0}] {1,-42} {2}" -f $status, $c.Name, $c.Detail)
}

if ($failed.Count -gt 0) {
    Write-Error "$($failed.Count) calibration invariant check(s) failed."
}

Write-Host "All contcal5 pose-contract checks passed."