@echo off
setlocal
set "BH_PROMPT=%~dp0FIRST_RUN_PROMPT.txt"

if not exist "%BH_PROMPT%" (
    echo ERROR: FIRST_RUN_PROMPT.txt is missing next to this launcher.
    echo Re-extract the complete ZIP and try again.
    echo.
    pause
    exit /b 1
)

where clip.exe >nul 2>&1
if errorlevel 1 (
    echo Clipboard tool was not found. Opening the prompt in Notepad instead.
) else (
    type "%BH_PROMPT%" | clip.exe
    if errorlevel 1 (
        echo The prompt could not be copied, but it will still open in Notepad.
    ) else (
        echo First-run prompt copied to the clipboard.
    )
)

start "" notepad.exe "%BH_PROMPT%"
echo Paste the prompt into Codex after opening the Broken Horizon project folder.
echo.
pause
exit /b 0
