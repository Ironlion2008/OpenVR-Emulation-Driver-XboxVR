param(
    [string]$Destination = (Join-Path $PSScriptRoot "..\libraries\openvr")
)

$ErrorActionPreference = "Stop"
$repo = "https://github.com/ValveSoftware/openvr.git"
$commit = "39205f6b281a6131d1373d0217c1ab9ed19735ea"
$destinationFull = [System.IO.Path]::GetFullPath($Destination)

if (Test-Path (Join-Path $destinationFull "headers\openvr_driver.h")) {
    Write-Host "OpenVR SDK already present: $destinationFull"
    exit 0
}

if (Test-Path $destinationFull) {
    Remove-Item $destinationFull -Recurse -Force
}

$parent = Split-Path $destinationFull -Parent
New-Item -ItemType Directory -Path $parent -Force | Out-Null

Write-Host "Fetching pinned OpenVR SDK..."
git clone --depth 1 $repo $destinationFull

git -C $destinationFull fetch --depth 1 origin $commit
git -C $destinationFull checkout $commit

if (!(Test-Path (Join-Path $destinationFull "headers\openvr_driver.h"))) {
    throw "OpenVR headers were not found after checkout."
}
if (!(Test-Path (Join-Path $destinationFull "lib\win64\openvr_api.lib"))) {
    throw "OpenVR win64 library was not found after checkout."
}

Write-Host "OpenVR SDK ready: $destinationFull"
