@echo off
setlocal
cd /d "%~dp0"
where pio >nul 2>nul
if errorlevel 1 (
  echo [LOI] Khong tim thay PlatformIO CLI ^(pio^).
  echo Hay cai extension PlatformIO IDE trong VS Code hoac chay: py -m pip install platformio
  pause
  exit /b 1
)
python tools\check_gpio.py || exit /b 1
pio run
if errorlevel 1 pause
