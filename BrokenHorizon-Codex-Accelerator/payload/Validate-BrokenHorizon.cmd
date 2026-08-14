@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\Validate.ps1" -ProjectRoot "%~dp0"
set "BH_EXIT=%ERRORLEVEL%"
echo.
pause
exit /b %BH_EXIT%
