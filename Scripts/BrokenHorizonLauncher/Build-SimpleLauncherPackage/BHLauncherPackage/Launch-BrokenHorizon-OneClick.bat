@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "PS_SCRIPT=%SCRIPT_DIR%Launch-BrokenHorizon-OneClick.ps1"

if not exist "%PS_SCRIPT%" (
    echo [Launcher] Missing file:
    echo %PS_SCRIPT%
    pause
    exit /b 1
)

echo [Launcher] Starting Broken Horizon updater...
set "LAUNCH_RESULT=0"
powershell -NoProfile -ExecutionPolicy Bypass -NoLogo -NonInteractive -File "%PS_SCRIPT%"
set "LAUNCH_RESULT=%errorlevel%"
if "%LAUNCH_RESULT%"=="0" goto launcher_done

if not "%LAUNCH_RESULT%"=="0" (
  echo [Launcher] Update process failed. Check message above.
)

:launcher_done

pause
exit /b %LAUNCH_RESULT%
