# Run from any folder: powershell -ExecutionPolicy Bypass -File .\tools\build_pio_windows.ps1
$ErrorActionPreference = 'Stop'
Set-Location (Resolve-Path (Join-Path $PSScriptRoot '..'))
if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 tools/build_pio.py
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    & python tools/build_pio.py
} else {
    Write-Error 'Python 3 not found. Install Python 3 and PlatformIO.'
    exit 2
}
exit $LASTEXITCODE
