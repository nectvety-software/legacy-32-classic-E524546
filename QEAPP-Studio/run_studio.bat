@echo off
setlocal
cd /d "%~dp0"
set PYTHONUTF8=1
where py >nul 2>nul
if %errorlevel%==0 (
  py -3 run_studio.py
) else (
  python run_studio.py
)
if errorlevel 1 (
  echo [ERROR] QEAPP Studio did not start. Install requirements:
  echo   py -3 -m pip install -r requirements-studio.txt
  pause
)
endlocal
