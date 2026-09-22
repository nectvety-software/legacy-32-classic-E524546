@echo off
setlocal
cd /d "%~dp0"
where pio >nul 2>nul
if errorlevel 1 (
  echo [LOI] Khong tim thay PlatformIO CLI ^(pio^).
  pause
  exit /b 1
)
python tools\check_gpio.py || exit /b 1
pio run -t upload
if errorlevel 1 pause
