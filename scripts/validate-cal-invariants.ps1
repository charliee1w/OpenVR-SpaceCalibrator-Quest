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

$scanIdx = $calCppText.IndexOf('void ScanAndApplyProfile(CalibrationContext &ctx)')
if ($scanIdx -lt 0) {
    $scanIdx = $calCppText.IndexOf('void ScanAndApplyProfile(CalibrationContext& ctx)')
}
$scanEnd = if ($scanIdx -ge 0) {
    $nextFn = [regex]::Match($calCppText.Substring($scanIdx + 1), '(?m)^void ').Index
    if ($nextFn -lt 0) { $calCppText.Length } else { $scanIdx + 1 + $nextFn }
} else { -1 }
$scanBody = if ($scanIdx -ge 0 -and $scanEnd -gt $scanIdx) {
    $calCppText.Substring($scanIdx, $scanEnd - $scanIdx)
} else { "" }
Add-Check "upstream apply gate in ScanAndApplyProfile" `
    ($scanBody -match 'ctx\.enabled = ctx\.validProfile') `
    "Apply enabled flag must follow upstream: validProfile only, no CanApply gate"
Add-Check "upstream device match in ScanAndApplyProfile" `
    ($scanBody -match 'trackingSystem != ctx\.targetTrackingSystem' `
        -and -not ($scanBody -match 'FindChainIndexForTargetSystem|ResolveChainForDevice|ResolveChainIndexForTargetSystem')) `
    "Device loop must match target tracking system directly, not via chain index lookup"
Add-Check "upstream transform source in ScanAndApplyProfile" `
    ($scanBody -match 'VRTranslationVec\(ctx\.calibratedTranslation\)') `
    "Saved profile offsets must come from ctx.calibratedTranslation, not live chain solver"
Add-Check "SyncCalCtxToPrimaryChain before apply scan" `
    ($scanBody -match 'SyncCalCtxToPrimaryChain') `
    "ScanAndApplyProfile syncs chain metadata before device loop (fork metadata only; does not change match logic)"

$failed = @($checks | Where-Object { -not $_.Pass })
foreach ($c in $checks) {
    $status = if ($c.Pass) { "PASS" } else { "FAIL" }
    Write-Host ("  [{0}] {1,-42} {2}" -f $status, $c.Name, $c.Detail)
}

if ($failed.Count -gt 0) {
    Write-Error "$($failed.Count) calibration invariant check(s) failed."
}

Write-Host "All contcal5 pose-contract checks passed."