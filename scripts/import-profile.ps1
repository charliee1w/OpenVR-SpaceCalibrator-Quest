# Import a starter or winner profile into the SpaceCal registry.
param(
    [ValidateSet('quest-vd', 'pico-alvr', 'quest-link', 'p4-winner')]
    [string]$Preset,
    [string]$ProfilePath,
    [switch]$Merge,
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$regKey = "HKCU:\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator"

$presets = @{
    'quest-vd'   = 'profiles\starter-quest-vd-lighthouse.json'
    'pico-alvr'  = 'profiles\starter-pico-alvr-lighthouse.json'
    'quest-link' = 'profiles\starter-quest-link-lighthouse.json'
    'p4-winner'  = 'profiles\example-quest-lighthouse.json'
}

if (-not $ProfilePath -and -not $Preset) {
    & (Join-Path $PSScriptRoot "list-starter-profiles.ps1")
    Write-Host ""
    Write-Host "Usage: .\scripts\import-profile.ps1 -Preset quest-vd" -ForegroundColor Cyan
    Write-Host "       .\scripts\import-profile.ps1 -Preset p4-winner -Merge"
    exit 0
}

if (-not $ProfilePath) {
    if (-not $presets.ContainsKey($Preset)) {
        Write-Error "Unknown preset '$Preset'. Valid: $($presets.Keys -join ', ')"
    }
    $ProfilePath = Join-Path $repoRoot $presets[$Preset]
}

if (-not (Test-Path $ProfilePath)) {
    Write-Error "Profile not found: $ProfilePath"
}

$incoming = @(Get-Content -Raw -Path $ProfilePath | ConvertFrom-Json)
if ($incoming.Count -lt 1) {
    Write-Error "Profile must be a non-empty JSON array."
}

$liveRaw = (Get-ItemProperty -Path $regKey -Name Config -ErrorAction SilentlyContinue).Config
$useMerge = $Merge -and -not [string]::IsNullOrWhiteSpace($liveRaw)

if ($useMerge) {
    $live = @($liveRaw | ConvertFrom-Json)
    if ($live.Count -lt 1) {
        Write-Error "Live profile invalid; import without -Merge or calibrate in overlay first."
    }
    $tuningKeys = @(
        'alignment_params', 'autostart_continuous_calibration', 'calibration_speed',
        'continuous_calibration_target_offset_x', 'continuous_calibration_target_offset_y',
        'continuous_calibration_target_offset_z', 'ignore_outliers', 'jitter_threshold',
        'lock_relative_position', 'max_relative_error_threshold', 'quash_target_in_continuous',
        'require_trigger_press_to_apply', 'static_calibration', 'max_pose_time_skew',
        'max_reference_pose_time_offset', 'compensate_pose_time_offset', 'trust_target_yaw',
        'reject_yaw_drift_poses', 'apply_head_model_to_reference', 'pause_on_reference_jitter',
        'auto_recal_on_guardian_drift', 'guardian_drift_trans_threshold_m',
        'guardian_drift_yaw_threshold_deg', 'guardian_drift_confirm_checks',
        'guardian_drift_cooldown_frames'
    )
    $preserveKeys = @(
        'chain_name', 'reference_tracking_system', 'target_tracking_system',
        'reference_device', 'target_device', 'roll', 'yaw', 'pitch', 'x', 'y', 'z', 'scale',
        'relative_transform', 'relative_pos_calibrated', 'chaperone'
    )
    $merged = @{}
    foreach ($prop in $live[0].PSObject.Properties) { $merged[$prop.Name] = $prop.Value }
    foreach ($key in $tuningKeys) {
        if ($incoming[0].PSObject.Properties.Name -contains $key) {
            $merged[$key] = $incoming[0].$key
        }
    }
    foreach ($key in $preserveKeys) {
        if ($live[0].PSObject.Properties.Name -contains $key) {
            $merged[$key] = $live[0].$key
        }
    }
    $outChains = [System.Collections.Generic.List[object]]::new()
    $outChains.Add([pscustomobject]$merged)
    for ($i = 1; $i -lt $live.Count; $i++) { $outChains.Add($live[$i]) }
    $outJson = ($outChains.ToArray() | ConvertTo-Json -Depth 20 -Compress:$false)
    Write-Host "Merge import: tuning from $ProfilePath into live profile" -ForegroundColor Cyan
} else {
    $outJson = (Get-Content -Raw -Path $ProfilePath)
    Write-Host "Fresh import: $ProfilePath" -ForegroundColor Cyan
    Write-Host "  Placeholder serials — run room calibration in VR before expecting apply." -ForegroundColor DarkYellow
}

$chainName = $incoming[0].chain_name
if ($chainName) { Write-Host "  chain: $chainName" }
Write-Host "  ref:   $($incoming[0].reference_tracking_system) -> $($incoming[0].target_tracking_system)"

if ($WhatIf) {
    Write-Host "WhatIf: registry not modified." -ForegroundColor Yellow
    exit 0
}

if (-not (Test-Path $regKey)) {
    New-Item -Path $regKey -Force | Out-Null
}
Set-ItemProperty -Path $regKey -Name Config -Value $outJson
Write-Host "Profile saved to registry. Restart SteamVR." -ForegroundColor Green