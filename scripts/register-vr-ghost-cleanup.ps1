# Register a background task that removes VRChat/SteamVR ghosts when Steam is not running.
param(
    [switch]$Unregister
)

$taskName = 'SpaceCal-VRGhostCleanup'
$scriptPath = Join-Path $PSScriptRoot 'watch-vr-ghosts.ps1'

if ($Unregister) {
    schtasks /Delete /TN $taskName /F 2>$null | Out-Null
    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
    Write-Host "Removed scheduled task: $taskName"
    exit 0
}

if (-not (Test-Path $scriptPath)) {
    Write-Error "Missing $scriptPath"
    exit 1
}

$tr = "powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$scriptPath`""

# schtasks is more reliable than Register-ScheduledTask on locked-down profiles.
schtasks /Delete /TN $taskName /F 2>$null | Out-Null
$result = schtasks /Create /TN $taskName /TR $tr /SC MINUTE /MO 5 /F 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to register task: $result"
    exit 1
}

Write-Host "Registered task '$taskName' (every 5 min)."
Write-Host "  Script: $scriptPath"
Write-Host "Manual full cleanup: .\scripts\kill-vr-ghosts.ps1 -ShutdownSteam"
Write-Host "Duplicates only:     .\scripts\kill-vr-ghosts.ps1 -DuplicatesOnly"
Write-Host "Unregister:          .\scripts\register-vr-ghost-cleanup.ps1 -Unregister"