@echo off
REM Sign a QEAPP project under Documents\QEAPP-Studio Projects with the starter key.
REM Usage: build_signed.bat <project-folder-name> [key-id]
setlocal
set STUDIO=D:\Program\arduino\legacy-32-classic-E524546\QEAPP-Studio
set KEYS=D:\Program\arduino\legacy-32-classic-E524546\QEAPP_Signing_Key_Starter
set ROOT=C:\Users\doxuanhop\Documents\QEAPP-Studio Projects
set NAME=%~1
set KEYID=%~2
if "%NAME%"=="" echo Usage: build_signed.bat doodle-notebook [0x31534351] & exit /b 2
if "%KEYID%"=="" set KEYID=0x31534351
set PROJ=%ROOT%\%NAME%
set PY=%STUDIO%\.venv\Scripts\python.exe
if not exist "%PY%" set PY=py -3
for /f "usebackq delims=" %%i in (`%PY% -c "import json;print(json.load(open(r'%PROJ%\qeapp.project.json',encoding='utf-8'))['id'])"`) do set APPID=%%i
set OUT=%PROJ%\dist\%APPID%.qeapp
set EXTRA=
%PY% -c "import json;raise SystemExit(0 if json.load(open(r'%PROJ%\qeapp.project.json',encoding='utf-8'))['type']=='lua' else 1)" && set EXTRA=--experimental-lua
echo Building %PROJ% -> %OUT%
%PY% "%STUDIO%\tools\qstudio.py" build "%PROJ%" --firmware-root "%STUDIO%\firmware\VQEAF-OS" --sign-key "%KEYS%\qeapp_private.pem" --key-id %KEYID% %EXTRA% -o "%OUT%"
if errorlevel 1 exit /b 1
%PY% "%STUDIO%\tools\qstudio.py" inspect "%OUT%" --public-key "%KEYS%\qeapp_public.pem" --key-id %KEYID%
endlocal
