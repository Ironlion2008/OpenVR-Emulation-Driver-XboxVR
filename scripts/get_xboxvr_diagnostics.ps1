$ErrorActionPreference = 'Stop'

$log = Join-Path ${env:ProgramFiles(x86)} 'Steam\logs\vrserver.txt'
if (!(Test-Path $log)) {
    Write-Error "SteamVR vrserver.txt not found: $log"
}

$out = Join-Path $env:USERPROFILE 'Desktop\xboxvr-diagnostic.txt'

$matches = Select-String -Path $log -Pattern '\[XboxVR\]'
if (!$matches) {
    Write-Host 'No [XboxVR] diagnostic lines were found in vrserver.txt.'
    Write-Host 'Restart SteamVR, wait a few seconds, then run this script again.'
    exit 2
}

$matches | Set-Content -Path $out -Encoding UTF8

Write-Host "XboxVR diagnostic log created:"
Write-Host $out
Write-Host ""
Write-Host "Last diagnostic lines:"
Get-Content $out -Tail 80
Write-Host ""
Write-Host "Open the file with:"
Write-Host "notepad `"$out`""
