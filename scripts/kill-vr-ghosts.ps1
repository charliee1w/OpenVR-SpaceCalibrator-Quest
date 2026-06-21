# Kill duplicate VRChat / SteamVR / SpaceCal processes left behind by Steam.
param(
    [switch]$DuplicatesOnly,
    [switch]$ShutdownSteam,
    [switch]$WhatIf
)

$ErrorActionPreference = 'SilentlyContinue'

function Write-Action([string]$Message) {
    Write-Host $Message -ForegroundColor Cyan
}

function Stop-ProcessSafe([System.Diagnostics.Process]$Proc, [string]$Reason) {
    if ($WhatIf) {
        Write-Host "  [whatif] kill $($Proc.ProcessName) pid=$($Proc.Id) - $Reason"
        return
    }
    try {
        Stop-Process -Id $Proc.Id -Force -ErrorAction Stop
        Write-Host "  killed $($Proc.ProcessName) pid=$($Proc.Id) - $Reason" -ForegroundColor Yellow
    } catch {
        Write-Host "  failed $($Proc.ProcessName) pid=$($Proc.Id) - $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Remove-DuplicateProcesses([string[]]$Names) {
    foreach ($name in $Names) {
        $procs = @(Get-Process -Name $name -ErrorAction SilentlyContinue)
        if ($procs.Count -le 1) { continue }

        $sorted = $procs | Sort-Object StartTime -Descending
        $keep = $sorted[0]
        Write-Action "Duplicate $($name): keeping pid=$($keep.Id) (newest), removing $($procs.Count - 1) extra"
        foreach ($extra in ($sorted | Select-Object -Skip 1)) {
            Stop-ProcessSafe $extra "duplicate $name"
        }
    }
}

function Remove-OrphanProcesses([string[]]$Names, [string]$Reason) {
    foreach ($name in $Names) {
        foreach ($proc in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
            Stop-ProcessSafe $proc $Reason
        }
    }
}

# VRChat on D: library; also match default Steam path.
$vrChatNames = @('VRChat', 'VRChat_Steam')
$steamVrNames = @(
    'vrserver', 'vrmonitor', 'vrcompositor', 'vrdashboard', 'vrstartup',
    'vrwebhelper', 'vrserverhelper', 'vrservice', 'vrprismhost'
)
$overlayNames = @('SpaceCalibrator', 'gameoverlayui64', 'gameoverlayui')

if ($ShutdownSteam) {
    Write-Action 'Asking Steam to shut down cleanly (steam.exe -shutdown)...'
    $steam = Get-Process -Name 'steam' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($steam) {
        $steamPath = $steam.Path
        if (-not $WhatIf) {
            Start-Process -FilePath $steamPath -ArgumentList '-shutdown' -WindowStyle Hidden
            Start-Sleep -Seconds 8
        } else {
            Write-Host '  [whatif] steam.exe -shutdown'
        }
    }
}

Write-Action 'Removing duplicate VRChat instances (keep newest only)...'
Remove-DuplicateProcesses $vrChatNames

if (-not $DuplicatesOnly) {
    Write-Action 'Killing SteamVR / overlay ghosts...'
    Remove-OrphanProcesses $steamVrNames 'SteamVR ghost'
    Remove-OrphanProcesses $overlayNames 'overlay ghost'

    # Second pass: any VRChat still running after SteamVR is gone = ghost.
    $vrRunning = @(Get-Process -Name $vrChatNames -ErrorAction SilentlyContinue)
    $svrRunning = @(Get-Process -Name 'vrserver' -ErrorAction SilentlyContinue)
    if ($vrRunning.Count -gt 0 -and $svrRunning.Count -eq 0) {
        Write-Action "VRChat running without vrserver - treating as ghost"
        Remove-OrphanProcesses $vrChatNames "VRChat without SteamVR"
    }
}

$remaining = @(
    (Get-Process -Name ($vrChatNames + $steamVrNames + $overlayNames) -ErrorAction SilentlyContinue)
) | Group-Object ProcessName | ForEach-Object {
    [pscustomobject]@{ Name = $_.Name; Count = $_.Count }
}

if ($remaining.Count -eq 0) {
    Write-Host "`nNo VRChat / SteamVR / SpaceCal ghosts remain." -ForegroundColor Green
} else {
    Write-Host "`nStill running:" -ForegroundColor Yellow
    $remaining | ForEach-Object { Write-Host "  $($_.Name): $($_.Count)" }
}