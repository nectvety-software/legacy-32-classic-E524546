$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$workspace = Resolve-Path (Join-Path $root "..\..")
$python = "C:\Users\admin\.platformio\penv\Scripts\python.exe"
$cc65 = Join-Path $workspace "tools\cc65-linux\root\usr\bin"
$wslRoot = "/mnt/d/Program/arduino/legacy-32-classic-E524546/pochita"
$wslGame = "$wslRoot/games/openrhynn-nes"
$wslTools = "$wslRoot/tools/cc65-linux/root/usr/bin"

& $python (Join-Path $root "generate_assets.py")
if ($LASTEXITCODE -ne 0) { throw "Asset generation failed" }

wsl.exe bash -lc "cd '$wslGame' && '$wslTools/ca65' -g -o openrhynn.o openrhynn.asm && '$wslTools/ld65' -C nes.cfg -m openrhynn.map -o OpenRhynn.nes openrhynn.o"
if ($LASTEXITCODE -ne 0) { throw "cc65 build failed" }

$rom = Join-Path $root "OpenRhynn.nes"
$bytes = [System.IO.File]::ReadAllBytes($rom)
if ($bytes.Length -ne 40976) { throw "Unexpected ROM size: $($bytes.Length)" }
if ($bytes[0] -ne 0x4E -or $bytes[1] -ne 0x45 -or $bytes[2] -ne 0x53 -or $bytes[3] -ne 0x1A) {
    throw "Invalid iNES header"
}

$destination = Join-Path $workspace "sd_card\roms\nes"
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item -Force $rom (Join-Path $destination "OpenRhynn.nes")

$py65 = Join-Path $workspace "tools\py65"
if (Test-Path $py65) {
    $env:PYTHONPATH = $py65
    & $python (Join-Path $root "smoke_test.py")
    if ($LASTEXITCODE -ne 0) { throw "6502 smoke test failed" }
}

Write-Host "Built valid iNES ROM: $rom"
Write-Host "Copied to: $destination"
