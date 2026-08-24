@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Prepare-G17R1-Client-Patch.ps1"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo G17-R1 client Spell.dbc staging completed.
) else (
  echo G17-R1 client staging stopped with error code %RC%.
)
echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17R1_CLIENT_PREPARE_RESULT.txt
pause
exit /b %RC%
