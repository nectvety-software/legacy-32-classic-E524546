@echo off
rem Use --dry-run to simulate all stages without PlatformIO or ESP32-S3 toolchain.
rem Examples: build_offline.bat --dry-run [--buildfs] [--simulate-failure linker]
setlocal EnableExtensions EnableDelayedExpansion
pushd "%~dp0.." || (echo [ERROR] Cannot open project directory & exit /b 2)
where py >nul 2>nul
if not errorlevel 1 (
  py -3 --version >nul 2>nul
  if not errorlevel 1 (
    py -3 tools\build_offline.py %*
    set "rc=!ERRORLEVEL!"
    goto done
  )
)
where python >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Python 3 is missing. Prepare Python and PlatformIO on an internet-connected computer.
  set "rc=2"
) else (
  python tools\build_offline.py %*
  set "rc=!ERRORLEVEL!"
)
:done
popd
exit /b %rc%
