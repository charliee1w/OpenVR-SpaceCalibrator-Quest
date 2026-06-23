# Merge P4 winner slider settings into the live registry profile.
# Preserves room calibration, device serials, relative pose, and chaperone.
param(
    [string]$WinnerPath,
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$WinnerPath = if ($WinnerPath) { $WinnerPath } else { Join-Path $repoRoot "profiles\example-quest-lighthouse.json" }
$regKey = "HKCU:\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator"

if (-not (Test-Path $WinnerPath)) {
    Write-Error "Winner template not found: $WinnerPath"
}

$winner = @(Get-Content -Raw -Path $WinnerPath | ConvertFrom-Json)
if ($winner.Count -lt 1) {
    Write-Error "Winner template must be a non-empty JSON array."
}
$winnerPrimary = $winner[0]

$liveRaw = (Get-ItemProperty -Path $regKey -Name Config -ErrorAction SilentlyContinue).Config
if ([string]::IsNullOrWhiteSpace($liveRaw)) {
    Write-Error "No live profile in registry. Calibrate once in the overlay, then re-run."
}

$live = @($liveRaw | ConvertFrom-Json)
if ($live.Count -lt 1) {
    Write-Error "Live profile JSON is empty or invalid."
}
$livePrimary = $live[0]

$preserveKeys = @(
    'chain_name',
    'reference_tracking_system',
    'target_tracking_system',
    'reference_device',
    'target_device',
    'roll', 'yaw', 'pitch', 'x', 'y', 'z', 'scale',
    'relative_transform',
    'relative_pos_calibrated',
    'chaperone'
)

$tuningKeys = @(
    'alignment_params',
    'autostart_continuous_calibration',
    'calibration_speed',
    'continuous_calibration_target_offset_x',
    'continuous_calibration_target_offset_y',
    'continuous_calibration_target_offset_z',
    'ignore_outliers',
    'jitter_threshold',
    'lock_relative_position',
    'max_relative_error_threshold',
    'quash_target_in_continuous',
    'require_trigger_press_to_apply',
    'static_calibration'
)

$merged = @{}
foreach ($prop in $livePrimary.PSObject.Properties) {
    $merged[$prop.Name] = $prop.Value
}
foreach ($key in $tuningKeys) {
    if ($winnerPrimary.PSObject.Properties.Name -contains $key) {
        $merged[$key] = $winnerPrimary.$key
    }
}
foreach ($key in $preserveKeys) {
    if ($livePrimary.PSObject.Properties.Name -contains $key) {
        $merged[$key] = $livePrimary.$key
    }
}

$outChains = [System.Collections.Generic.List[object]]::new()
$outChains.Add([pscustomobject]$merged)
for ($i = 1; $i -lt $live.Count; $i++) {
    $outChains.Add($live[$i])
}
$outJson = ($outChains.ToArray() | ConvertTo-Json -Depth 20 -Compress:$false)

Write-Host "P4 winner merge (primary chain only)" -ForegroundColor Cyan
Write-Host "  winner: $WinnerPath"
Write-Host "  ref:    $($livePrimary.reference_device.serial) ($($livePrimary.reference_tracking_system))"
Write-Host "  target: $($livePrimary.target_device.serial) ($($livePrimary.target_tracking_system))"
Write-Host "  room:   x=$($livePrimary.x) y=$($livePrimary.y) z=$($livePrimary.z)"
Write-Host "  tuning: lock_relative=$($merged.lock_relative_position) jitter=$($merged.jitter_threshold) max_rel_err=$($merged.max_relative_error_threshold)"

if ($WhatIf) {
    Write-Host "`nWhatIf: registry not modified." -ForegroundColor Yellow
    exit 0
}

if (-not (Test-Path $regKey)) {
    New-Item -Path $regKey -Force | Out-Null
}
Set-ItemProperty -Path $regKey -Name Config -Value $outJson
Write-Host "`nProfile saved. Restart SteamVR or re-save in overlay to pick up changes." -ForegroundColor Green