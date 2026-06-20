# Apply P4 winner slider settings to the live SpaceCal registry profile.
# Preserves your device serials and saved calibration transform (x/y/z/yaw/pitch/roll).
param(
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"
$regKey = "HKCU:\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator"

if (-not (Test-Path $regKey)) {
    Write-Error "SpaceCal registry profile not found. Calibrate once in VR first."
}

$raw = (Get-ItemProperty $regKey).Config
if (-not $raw) {
    Write-Error "Config property is empty."
}

$profiles = $raw | ConvertFrom-Json
if ($profiles.Count -lt 1) {
    Write-Error "Expected at least one profile chain in Config."
}

$p = $profiles[0]

# P4 winner — Quest Pro + VD + Tundra head (contcal5 SLAM preset values)
$p.autostart_continuous_calibration = $true
$p.lock_relative_position = $true
$p.jitter_threshold = 0.15
$p.max_relative_error_threshold = 0.008
$p.continuous_calibration_target_offset_x = 0
$p.continuous_calibration_target_offset_y = 0
$p.continuous_calibration_target_offset_z = 0
if ($p.alignment_params) {
    $p.alignment_params.continuousCalibrationThreshold = 1.5
}
$p.static_calibration = $true
$p.quash_target_in_continuous = $true
$p.ignore_outliers = $false
$p.calibration_speed = 1

$out = ($profiles | ConvertTo-Json -Depth 20 -Compress:$false)

Write-Host "P4 winner settings:"
Write-Host "  autostart_continuous_calibration: true"
Write-Host "  lock_relative_position: true"
Write-Host "  jitter_threshold: 0.15"
Write-Host "  max_relative_error_threshold: 0.008"
Write-Host "  continuous_calibration_target_offset: 0,0,0"
Write-Host "  continuousCalibrationThreshold: 1.5"
Write-Host "  devices/transform: unchanged"

if ($WhatIf) {
    Write-Host "`nWhatIf — registry not modified."
    exit 0
}

Set-ItemProperty $regKey -Name Config -Value $out
Write-Host "`nApplied. Restart SteamVR (or save profile in overlay) to load."