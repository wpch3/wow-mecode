@echo off
setlocal
cd /d "%~dp0"
echo [G17-R4] Close Wow.exe before continuing.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17R4-Client-MPQ.ps1"
set RC=%ERRORLEVEL%
echo.
if not "%RC%"=="0" (
  echo G17-R4 failed. Read C:\Users\Administrator\Downloads\workspace\uploads\G17R4_CLIENT_MPQ_UPGRADE_RESULT.txt
) else (
  echo G17-R4 client patch installed. Start WoW and verify Wetlands IsFlyableArea plus spell 59961.
)
pause
exit /b %RC%
