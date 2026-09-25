@echo off
setlocal
cd /d "%~dp0.."
echo [VQEAF] Verifying identical RGB565 output on host...
where g++ >nul 2>&1
if not errorlevel 1 (
    py -3 tools\verify_icon_device_v236.py
    if errorlevel 1 (
        echo [FAIL] Host verification failed, hardware flashing aborted.
        exit /b 1
    )
) else (
    echo [INFO] g++ unavailable. Skipping optional host check, continuing to PlatformIO.
)
echo [VQEAF] Building ESP32-S3 diagnostic image...
pio run -e vqeaf_icon_selftest
if errorlevel 1 (
    echo [FAIL] Target compile/link failed, read PlatformIO output above.
    exit /b 1
)
echo [OK] Diagnostic image is built. Flash explicitly with:
echo pio run -e vqeaf_icon_selftest -t upload
echo [INFO] Then run: py -3 tools\capture_icon_selftest.py --port COM5
endlocal
