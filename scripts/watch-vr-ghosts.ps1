# Background cleanup: always dedupe VRChat; kill VR stack ghosts when Steam is down.
$ErrorActionPreference = 'SilentlyContinue'

$vrChatNames = @('VRChat', 'VRChat_Steam')
$steamVrNames = @(
    'vrserver', 'vrmonitor', 'vrcompositor', 'vrdashboard', 'vrstartup',
    'vrwebhelper', 'vrserverhelper', 'vrservice', 'vrprismhost'
)
$overlayNames = @('SpaceCalibrator', 'gameoverlayui64', 'gameoverlayui')

function Stop-All([string[]]$Names) {
    foreach ($name in $Names) {
        Get-Process -Name $name -ErrorAction SilentlyContinue | Stop-Process -Force
    }
}

function Remove-DuplicateProcesses([string[]]$Names) {
    foreach ($name in $Names) {
        $procs = @(Get-Process -Name $name -ErrorAction SilentlyContinue)
        if ($procs.Count -le 1) { continue }

        $sorted = $procs | Sort-Object StartTime -Descending
        foreach ($extra in ($sorted | Select-Object -Skip 1)) {
            Stop-Process -Id $extra.Id -Force
        }
    }
}

# Always safe: keep newest VRChat, kill extras (common after crash/relaunch).
Remove-DuplicateProcesses $vrChatNames

$steamRunning = [bool](Get-Process -Name 'steam' -ErrorAction SilentlyContinue)
if ($steamRunning) {
    # Steam up but VRChat orphaned (no vrserver) = ghost from bad SteamVR exit.
    $vrRunning = @(Get-Process -Name $vrChatNames -ErrorAction SilentlyContinue)
    $svrRunning = @(Get-Process -Name 'vrserver' -ErrorAction SilentlyContinue)
    if ($vrRunning.Count -gt 0 -and $svrRunning.Count -eq 0) {
        Stop-All $vrChatNames
    }
    exit 0
}

# Steam is down: kill entire VR stack + overlays + any leftover VRChat.
Stop-All ($steamVrNames + $overlayNames + $vrChatNames)