# build_sim.ps1 — build qeafbrowser_sim.exe (MinGW g++, static)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$gpp = "C:\msys64\mingw64\bin\g++.exe"
if (-not (Test-Path $gpp)) { $gpp = "g++" }
$env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + $env:PATH

Set-Location $PSScriptRoot

& $gpp -O2 -std=gnu++17 -static -D_POSIX_THREAD_SAFE_FUNCTIONS=1 -I shims -I "$root\include" `
  -o qeafbrowser_sim.exe `
  sim_main.cpp sim_arduino.cpp `
  "$root\src\main.cpp" "$root\src\wml.cpp" "$root\src\http.cpp" "$root\src\store.cpp" `
  -lws2_32 -lgdi32 -lcomdlg32 -lcomdlg32

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "OK -> $PSScriptRoot\qeafbrowser_sim.exe"
