param(
    [switch]$Release
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

Write-Host "==============================================="
Write-Host " XboxVR OpenVR Emulator bootstrap"
Write-Host "==============================================="

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found on PATH. Install CMake or use GitHub Actions to build."
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git was not found on PATH."
}

Write-Host "Checking pinned OpenVR SDK..."
& (Join-Path $repoRoot "scripts\fetch_openvr.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "OpenVR SDK setup failed."
}

$buildDir = Join-Path $repoRoot "build"
if (Test-Path $buildDir) {
    Remove-Item $buildDir -Recurse -Force
}

$generator = "Visual Studio 17 2022"
$config = if ($Release) { "Release" } else { "Debug" }
$openvrRoot = Join-Path $repoRoot "libraries\openvr"

Write-Host "Configuring $config with $generator..."
cmake -S $repoRoot -B $buildDir `
    -G $generator `
    -A x64 `
    "-DOPENVR_ROOT=$openvrRoot" `
    -DBUILD_PREVIEW_APP=OFF `
    -DBUILD_TESTING=OFF `
    -DOPENVR_EMULATOR_SKIP_DEPLOY=ON

if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed."
}

Write-Host "Building driver..."
cmake --build $buildDir --config $config --target driver_openvr-emulator --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Driver build failed."
}

Write-Host ""
Write-Host "Build completed successfully."
Write-Host "Driver output: $buildDir\$config\openvr-emulator"
