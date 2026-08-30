@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C8 Per-Slot Visuals + Cooldown Display (client+server) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C8-PerSlot-Visuals.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C8] INSTALL FAILED.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17C8_RESULT.txt
) else (
  echo [G17C8] INSTALL PASSED. Restart both worldserver and WoW client.
)
echo.
pause
exit /b %RC%
