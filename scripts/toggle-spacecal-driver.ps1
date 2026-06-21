# Toggle 01spacecalibrator driver on/off for lag A/B. Restart SteamVR after running.
param(
    [string]$SteamRoot = "${env:ProgramFiles(x86)}\Steam\steamapps\common",
    [ValidateSet("disable", "enable", "status")]
    [string]$Action = "status"
)

$ErrorActionPreference = "Stop"
$manifest = Join-Path $SteamRoot "SteamVR\drivers\01spacecalibrator\driver.vrdrivermanifest"
$disabled = "$manifest.disabled"

switch ($Action) {
    "status" {
        if (Test-Path $manifest) { Write-Host "ENABLED: $manifest" }
        elseif (Test-Path $disabled) { Write-Host "DISABLED: $disabled" }
        else { Write-Host "NOT INSTALLED" }
    }
    "disable" {
        if (-not (Test-Path $manifest)) {
            if (Test-Path $disabled) { Write-Host "Already disabled: $disabled"; exit 0 }
            Write-Error "Manifest not found: $manifest"
        }
        Rename-Item $manifest $disabled -Force
        Write-Host "Disabled Space Calibrator driver. Restart SteamVR."
    }
    "enable" {
        if (-not (Test-Path $disabled)) {
            if (Test-Path $manifest) { Write-Host "Already enabled: $manifest"; exit 0 }
            Write-Error "Disabled manifest not found: $disabled"
        }
        Rename-Item $disabled $manifest -Force
        Write-Host "Enabled Space Calibrator driver. Restart SteamVR."
    }
}