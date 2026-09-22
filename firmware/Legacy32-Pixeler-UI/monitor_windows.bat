@echo off
setlocal
cd /d "%~dp0"
where pio >nul 2>nul
if errorlevel 1 (
  echo [LOI] Khong tim thay PlatformIO CLI ^(pio^).
  pause
  exit /b 1
)
pio device monitor -b 115200
