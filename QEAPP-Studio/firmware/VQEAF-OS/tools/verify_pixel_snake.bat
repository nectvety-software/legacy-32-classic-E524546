@echo off
setlocal
pushd "%~dp0\.."
if errorlevel 1 exit /b 1
where py >nul 2>&1
if errorlevel 1 (
  echo [ERROR] Python Launcher py -3 not found
  popd
  exit /b 10
)
echo [CHECK] GNU G++ / OpenSSL development headers / Python Pillow required.
py -3 tools\test_pixel_snake.py
if errorlevel 1 (
  echo [FAIL] Pixel Snake game or signed-package validation.
  popd
  exit /b 1
)
echo [CHECK] VQEAF v2.4.0 regression suite.
py -3 tools\verify_v240.py
if errorlevel 1 (
  echo [FAIL] Base firmware regression.
  popd
  exit /b 2
)
echo [PASS] Host simulation. ESP32 build must be run separately with PlatformIO.
popd
exit /b 0
