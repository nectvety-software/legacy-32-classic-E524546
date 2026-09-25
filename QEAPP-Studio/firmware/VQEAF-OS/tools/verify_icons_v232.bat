@echo off
setlocal
cd /d "%~dp0.."
py -3 tools\test_icon_integration_v232.py
if errorlevel 1 exit /b 1
echo PASS: VQEAF OS v2.3.2 icon integration host gate
