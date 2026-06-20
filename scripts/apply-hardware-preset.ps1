# Merge a hardware preset JSON into the live registry profile (sliders + tuning only).
# Preserves saved calibration transform and device serials from your existing profile.
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('tundra', 'vive3')]
    [string]$HeadTracker,
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$presetFile = switch ($HeadTracker) {
    'tundra' { Join-Path $repoRoot "profiles\preset-quest-tundra-head.json" }
    'vive3'  { Join-Path $repoRoot "profiles\preset-quest-vive3-head.json" }
}

$regKey = "HKCU:\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator"
if (-not (Test-Path $regKey)) {
    Write-Error "SpaceCal registry profile not found. Calibrate once in VR first."
}

$live = (Get-ItemProperty $regKey).Config | ConvertFrom-Json
$preset = Get-Content $presetFile -Raw | ConvertFrom-Json

$keep = @{
    x = $live[0].x; y = $live[0].y; z = $live[0].z
    roll = $live[0].roll; pitch = $live[0].pitch; yaw = $live[0].yaw
    scale = $live[0].scale
    reference_device = $live[0].reference_device
    target_device = $live[0].target_device
    relative_transform = $live[0].relative_transform
    relative_pos_calibrated = $live[0].relative_pos_calibrated
}

$merged = $preset[0]
foreach ($k in $keep.Keys) { $merged.$k = $keep[$k] }

$out = (@($merged) | ConvertTo-Json -Depth 20 -Compress:$false)
Write-Host "Preset: $presetFile"
Write-Host "Keeps: room transform + device serials from registry"

if ($WhatIf) {
    Write-Host "WhatIf — registry not modified."
    exit 0
}

Set-ItemProperty $regKey -Name Config -Value $out
Write-Host "Applied. Restart SteamVR or re-save profile in overlay."