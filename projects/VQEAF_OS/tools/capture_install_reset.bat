@echo off
setlocal
cd /d "%~dp0.."
where py >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Please install Python 3.10+ with the py launcher.
  pause
  exit /b 1
)
py -3 -c "import serial" >nul 2>nul
if errorlevel 1 (
  echo [INFO] Missing pyserial.
  echo Run: py -3 -m pip install -r tools\serial_requirements.txt
  pause
  exit /b 1
)
echo ================================================
echo VQEAF OS Serial Capture 115200 - Install Reboot
echo ================================================
py -3 tools\capture_install_reset.py --list-ports
echo.
set /p PORT=Enter COM port (e.g. COM5, or auto):
if "%PORT%"=="" set "PORT=auto"
echo.
echo 1. Start recording BEFORE installing a QEAPP.
echo 2. Press physical RESET once to capture ROM boot baseline.
echo 3. In this console enter i before Install. Use r before deliberate RESET.
echo 4. Keep recording after a crash and until following boot completes.
echo 5. Type q then Enter to save log and report.
echo.
set "DTR_ARG="
set "DTR="
set /p DTR=Use USB CDC that requires DTR? [y/N]:
if /i "%DTR%"=="y" set "DTR_ARG=--dtr"
py -3 -u tools\capture_install_reset.py --port "%PORT%" --out build_reports\device_serial %DTR_ARG%
set "CODE=%ERRORLEVEL%"
echo.
echo Logs are in: build_reports\device_serial\vqeaf_uart_*
pause
exit /b %CODE%
