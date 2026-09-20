param(
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Vendor = Join-Path $Root 'HIDMaestro'
$Dll = Join-Path $Vendor 'HIDMaestro.Core.dll'
$Tmp = Join-Path $env:TEMP 'XboxVR-HIDMaestro'

New-Item -ItemType Directory -Force -Path $Vendor, $Tmp | Out-Null

Write-Host '== XboxVR HIDMaestro setup ==' -ForegroundColor Cyan

$release = Invoke-RestMethod -Uri 'https://api.github.com/repos/hifihedgehog/HIDMaestro/releases/latest' -Headers @{ 'User-Agent'='XboxVR-HIDMaestroBridge' }
$asset = $release.assets | Where-Object { $_.name -match '\.zip$' } | Select-Object -First 1
if (-not $asset) { throw 'Could not find the HIDMaestro release ZIP.' }

$zip = Join-Path $Tmp $asset.name
Write-Host "Downloading HIDMaestro $($release.tag_name)..."
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zip -UseBasicParsing

$extract = Join-Path $Tmp 'extract'
if (Test-Path $extract) { Remove-Item $extract -Recurse -Force }
Expand-Archive -Path $zip -DestinationPath $extract -Force
$core = Get-ChildItem $extract -Filter 'HIDMaestro.Core.dll' -Recurse | Select-Object -First 1
if (-not $core) { throw 'HIDMaestro.Core.dll was not found in the release ZIP.' }
Copy-Item $core.FullName $Dll -Force

$sha = (Get-FileHash $Dll -Algorithm SHA256).Hash
Write-Host "HIDMaestro.Core.dll SHA256: $sha"

if (-not $SkipBuild) {
    $project = Join-Path $Root 'XboxVR-HIDMaestroBridge.csproj'
    dotnet build $project -c Release -r win-x64 --self-contained false
}

Write-Host ''
Write-Host 'Setup complete.' -ForegroundColor Green
Write-Host 'Run XboxVR-HIDMaestroBridge.exe as Administrator once so HIDMaestro can register its SteamVR driver.'
