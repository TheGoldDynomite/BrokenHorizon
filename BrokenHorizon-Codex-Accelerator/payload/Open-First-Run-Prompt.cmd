@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\OpenFirstRunPrompt.ps1" -ProjectRoot "%~dp0" -Open
set "BH_EXIT=%ERRORLEVEL%"
echo.
pause
exit /b %BH_EXIT%
