# Disable SpaceOverride SteamVR driver (reduces pose-hook overhead when not using HMD drive).
# Restart SteamVR after running.
param(
    [string]$SteamRoot = "${env:ProgramFiles(x86)}\Steam\steamapps\common"
)

$ErrorActionPreference = "Stop"
$manifest = Join-Path $SteamRoot "SteamVR\drivers\spaceoverride\driver.vrdrivermanifest"
$disabled = "$manifest.disabled"

if (-not (Test-Path $manifest)) {
    if (Test-Path $disabled) {
        Write-Host "SpaceOverride driver already disabled: $disabled"
        exit 0
    }
    Write-Host "SpaceOverride driver manifest not found (nothing to disable)."
    exit 0
}

Rename-Item $manifest $disabled -Force
Write-Host "Disabled SpaceOverride driver. Restart SteamVR."
Write-Host "  $disabled"