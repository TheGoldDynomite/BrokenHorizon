@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-BrokenHorizonCodexKit.ps1"
set "BH_EXIT=%ERRORLEVEL%"
echo.
if not "%BH_EXIT%"=="0" echo Installer returned exit code %BH_EXIT%.
pause
exit /b %BH_EXIT%
