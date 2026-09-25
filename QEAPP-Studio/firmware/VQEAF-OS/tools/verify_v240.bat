@echo off
setlocal
cd /d "%~dp0.."
py -3 tools\verify_v240.py
if errorlevel 1 (
  echo [FAIL] Review build_reports\v240\report.md
  exit /b 1
)
echo [PASS] Host release gates. PlatformIO and hardware require separate tests.
