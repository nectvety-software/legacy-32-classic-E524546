@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
set "VQEAF_COM=%~1"
if "%VQEAF_COM%"=="" set /p "VQEAF_COM=ESP32-S3 serial port (example COM5): "
if "%VQEAF_COM%"=="" (echo [ERROR] Missing COM port.&exit /b 2)
py -3 -c "import serial" >nul 2>&1
if errorlevel 1 (
  echo [ERROR] pyserial is missing. Install with: py -3 -m pip install pyserial
  exit /b 3
)
echo VQEAF OS v2.5.1: Close PlatformIO Serial Monitor and other COM port users.
echo Opening USB CDC may reset the board once. Type i ENTER just before install, b ENTER before game benchmark, q ENTER to stop.
py -3 tools\measure_v251_serial.py --port %VQEAF_COM% --baud 115200 --seconds 180 --interactive --out build_reports\device\v251_last
if errorlevel 1 (echo [ERROR] Measurement failed. Verify the device port and permissions.&exit /b 1)
echo [PASS] Report: build_reports\device\v251_last\report.md
endlocal
