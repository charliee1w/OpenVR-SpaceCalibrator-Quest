# Zip overlay + driver artifacts for GitHub release. Run after Release build.
param(
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$ver = Select-String -Path (Join-Path $repoRoot "src\common\Version.h") -Pattern 'SPACECAL_VERSION_STRING "(.+)"' |
    ForEach-Object { $_.Matches.Groups[1].Value }
$tag = $ver -replace 'gore-', ''

$release = Join-Path $repoRoot "bin\artifacts\Release"
$driverDll = Join-Path $repoRoot "bin\driver_01spacecalibrator\bin\win64\driver_01spacecalibrator.dll"
$manifest = Join-Path $repoRoot "driver_01spacecalibrator\driver.vrdrivermanifest"

foreach ($p in @(
    (Join-Path $release "SpaceCalibrator.exe"),
    $driverDll,
    $manifest
)) {
    if (-not (Test-Path $p)) {
        Write-Error "Missing: $p - run: cmake --build bin --config Release"
    }
}

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $repoRoot "bin\release"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$staging = Join-Path $env:TEMP "spacecal-release-$tag"
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $staging "bin\win64") | Out-Null

Copy-Item (Join-Path $release "SpaceCalibrator.exe") (Join-Path $staging "SpaceCalibrator.exe")
Copy-Item $driverDll (Join-Path $staging "bin\win64\driver_01spacecalibrator.dll")
Copy-Item $manifest (Join-Path $staging "driver.vrdrivermanifest")
Copy-Item (Join-Path $repoRoot "scripts\deploy.ps1") $staging
Copy-Item (Join-Path $repoRoot "scripts\validate-install.ps1") $staging

$zip = Join-Path $OutDir "OpenVR-SpaceCalibrator-$tag.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $zip -Force
Remove-Item $staging -Recurse -Force

Write-Host "Release package: $zip"
Write-Host "Tag: v$tag"
Write-Host ('gh release create v{0} "{1}" --title "{2}" --notes-file docs/CHANGELOG-contcal.md' -f $tag, $zip, $ver)