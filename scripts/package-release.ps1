# Zip overlay + driver artifacts for GitHub release. Run after Release build.
param(
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$ver = Select-String -Path (Join-Path $repoRoot "src\common\Version.h") -Pattern 'SPACECAL_VERSION_STRING "(.+)"' |
    ForEach-Object { $_.Matches.Groups[1].Value }
$tag = $ver -replace '^1\.5\.1-', '' -replace 'gore-', ''

function Get-BuildRoot([string]$Root) {
    $bestRoot = $null
    $bestTime = [datetime]::MinValue
    foreach ($name in @('build', 'bin')) {
        $candidate = Join-Path $Root $name
        $exe = Join-Path $candidate 'artifacts\Release\SpaceCalibrator.exe'
        if (-not (Test-Path $exe)) { continue }
        $written = (Get-Item $exe).LastWriteTime
        if ($written -gt $bestTime) {
            $bestTime = $written
            $bestRoot = $candidate
        }
    }
    if (-not $bestRoot) {
        Write-Error "No Release build found under build/ or bin/. Run: cmake --build build --config Release"
    }
    return $bestRoot
}

$buildRoot = Get-BuildRoot $repoRoot
$release = Join-Path $buildRoot 'artifacts\Release'
$driverDll = Join-Path $buildRoot 'driver_01spacecalibrator\bin\win64\driver_01spacecalibrator.dll'
$manifest = Join-Path $repoRoot 'driver_01spacecalibrator\driver.vrdrivermanifest'

foreach ($p in @(
    (Join-Path $release 'SpaceCalibrator.exe'),
    $driverDll,
    $manifest
)) {
    if (-not (Test-Path $p)) {
        Write-Error "Missing: $p"
    }
}

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $buildRoot 'release'
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$staging = Join-Path $env:TEMP "spacecal-release-$tag"
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $staging 'bin\win64') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $staging 'profiles') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $staging 'docs') | Out-Null

Copy-Item (Join-Path $release 'SpaceCalibrator.exe') (Join-Path $staging 'SpaceCalibrator.exe')
Copy-Item $driverDll (Join-Path $staging 'bin\win64\driver_01spacecalibrator.dll')
Copy-Item $manifest (Join-Path $staging 'driver.vrdrivermanifest')

$scriptNames = @(
    'deploy.ps1', 'validate-install.ps1', 'install.ps1', 'import-profile.ps1',
    'list-starter-profiles.ps1', 'apply-p4-winner.ps1', 'run-p4-validation.ps1',
    'analyze-spacecal-log.ps1'
)
foreach ($name in $scriptNames) {
    $src = Join-Path $repoRoot "scripts\$name"
    if (Test-Path $src) { Copy-Item $src $staging }
}

Get-ChildItem (Join-Path $repoRoot 'profiles') -Filter '*.json' | Copy-Item -Destination (Join-Path $staging 'profiles')
Copy-Item (Join-Path $repoRoot 'docs\QUICKSTART.md') (Join-Path $staging 'docs\QUICKSTART.md') -ErrorAction SilentlyContinue
Copy-Item (Join-Path $repoRoot 'README.md') $staging

$zip = Join-Path $OutDir "OpenVR-SpaceCalibrator-$tag.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $zip -Force
Remove-Item $staging -Recurse -Force

Write-Host "Release package: $zip"
Write-Host "Version: $ver"
Write-Host "Tag: v$tag"
Write-Host ""
Write-Host "End-user install from zip:"
Write-Host "  Expand zip, run: pwsh -File install.ps1 -Preset quest-vd"