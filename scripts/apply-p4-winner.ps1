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

function Set-ProfileProp($obj, [string]$Name, $Value) {
    $obj | Add-Member -NotePropertyName $Name -NotePropertyValue $Value -Force
}

# P4/P5 winner — Quest + VD + lighthouse head (contcal6 SLAM preset values)
Set-ProfileProp $p autostart_continuous_calibration $true
Set-ProfileProp $p lock_relative_position $true
Set-ProfileProp $p jitter_threshold 0.15
Set-ProfileProp $p max_relative_error_threshold 0.008
Set-ProfileProp $p continuous_calibration_target_offset_x 0
Set-ProfileProp $p continuous_calibration_target_offset_y 0
Set-ProfileProp $p continuous_calibration_target_offset_z 0
Set-ProfileProp $p static_calibration $true
Set-ProfileProp $p quash_target_in_continuous $true
Set-ProfileProp $p ignore_outliers $false
Set-ProfileProp $p calibration_speed 1
Set-ProfileProp $p continuous_spike_threshold_m 0.05
Set-ProfileProp $p guardian_drift_trans_threshold_m 0.035
Set-ProfileProp $p guardian_drift_yaw_threshold_deg 5
Set-ProfileProp $p guardian_drift_confirm_checks 3
Set-ProfileProp $p guardian_drift_cooldown_frames 60
Set-ProfileProp $p auto_recal_on_guardian_drift $true
if (-not $p.alignment_params) {
    $p | Add-Member -NotePropertyName alignment_params -NotePropertyValue ([pscustomobject]@{}) -Force
}
Set-ProfileProp $p.alignment_params continuousCalibrationThreshold 1.5
Set-ProfileProp $p.alignment_params align_rot_speed_scale 0.45

$out = ($profiles | ConvertTo-Json -Depth 20 -Compress:$false)

Write-Host "P4 winner settings:"
Write-Host "  autostart_continuous_calibration: true"
Write-Host "  lock_relative_position: true"
Write-Host "  jitter_threshold: 0.15"
Write-Host "  max_relative_error_threshold: 0.008"
Write-Host "  continuous_calibration_target_offset: 0,0,0"
Write-Host "  continuousCalibrationThreshold: 1.5"
Write-Host "  align_rot_speed_scale: 0.45"
Write-Host "  spike/guardian tuning: contcal6 defaults"
Write-Host "  devices/transform: unchanged"

if ($WhatIf) {
    Write-Host "`nWhatIf — registry not modified."
    exit 0
}

Set-ItemProperty $regKey -Name Config -Value $out
Write-Host "`nApplied. Restart SteamVR (or save profile in overlay) to load."