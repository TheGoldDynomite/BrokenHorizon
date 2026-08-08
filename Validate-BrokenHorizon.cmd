@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\Validate-BrokenHorizon.ps1" %*
exit /b %ERRORLEVEL%
