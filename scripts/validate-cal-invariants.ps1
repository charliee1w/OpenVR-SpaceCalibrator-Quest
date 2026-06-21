# Static checks for calibration pose-source invariants. Run before deploy / in CI.
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent

$checks = [System.Collections.Generic.List[object]]::new()

function Add-Check([string]$Name, [bool]$Pass, [string]$Detail) {
    $checks.Add([pscustomobject]@{ Name = $Name; Pass = $Pass; Detail = $Detail })
}

$calPoses = Join-Path $repoRoot "src\overlay\CalibrationPoses.cpp"
$calCpp = Join-Path $repoRoot "src\overlay\Calibration.cpp"
$driverCpp = Join-Path $repoRoot "src\driver\ServerTrackedDeviceProvider.cpp"

if (-not (Test-Path $calPoses)) {
    Write-Error "Missing $calPoses"
}

$calPosesText = Get-Content $calPoses -Raw
$calCppText = Get-Content $calCpp -Raw
$driverText = Get-Content $driverCpp -Raw

Add-Check "centralized pose refresh module" $true $calPoses

Add-Check "active transform blocks VRSystem target fallback" `
    ($calPosesText -match 'if \(!ctx\.validProfile \|\| !ctx\.relativePosCalibrated\)') `
    "CalibrationPoses.cpp must gate VRSystem target fallback until profile+relative pose are ready"

Add-Check "reference always VRSystem" `
    ($calPosesText -match 'VRSystemReference') `
    "reference pose must be tagged VRSystemReference"

Add-Check "cal tick uses RefreshDevicePosesForCalibration" `
    ($calCppText -match 'RefreshDevicePosesForCalibration') `
    "CalibrationTick must call RefreshDevicePosesForCalibration only"

Add-Check "saved profile scan on device change" `
    ($calCppText -match 'MaybeScanAndApplySavedProfile') `
    "continuous cal must re-apply profile when trackers connect"

Add-Check "no duplicate shmem read in Calibration.cpp" `
    (-not ($calCppText -match 'shmem\.ReadNewPoses')) `
    "pose shmem read must live only in CalibrationPoses.cpp"

Add-Check "shmem on every pose update" `
    ($driverText -match 'shmem\.SetPose\(openVRID, pose\)' -and -not ($driverText -match 'ShouldStreamPoseForCal')) `
    "driver must stream raw pre-transform poses every frame (no cal-rate throttle)"

Add-Check "shmem written before transform" `
    ($driverText -match 'shmem\.SetPose' -and $driverText -match 'tf\.enabled') `
    "driver must SetPose before applying enabled transform"

$failed = @($checks | Where-Object { -not $_.Pass })
foreach ($c in $checks) {
    $status = if ($c.Pass) { "PASS" } else { "FAIL" }
    Write-Host ("  [{0}] {1,-42} {2}" -f $status, $c.Name, $c.Detail)
}

if ($failed.Count -gt 0) {
    Write-Error "$($failed.Count) calibration invariant check(s) failed."
}

Write-Host "All calibration invariant checks passed."