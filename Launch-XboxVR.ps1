$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$bridge = Join-Path $root 'tools\XboxVR-HIDMaestroBridge\bin\Release\net10.0-windows10.0.19041.0\win-x64\XboxVR-HIDMaestroBridge.exe'
if (-not (Test-Path $bridge)) { throw "Bridge not built: $bridge. Run tools\XboxVR-HIDMaestroBridge\setup_hidmaestro.ps1 first." }

Start-Process 'steam://run/250820'
Start-Sleep -Seconds 5
Start-Process $bridge -Verb RunAs
