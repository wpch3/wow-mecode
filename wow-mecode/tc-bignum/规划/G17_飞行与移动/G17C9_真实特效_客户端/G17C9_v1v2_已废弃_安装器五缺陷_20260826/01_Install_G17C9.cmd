@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C9 Real Per-Skill Visuals + Remove Phantom Cooldown (client+server) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C9-Real-Visuals.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C9] INSTALL FAILED.
) else (
  echo [G17C9] INSTALL PASSED. Restart worldserver + WoW client.
)
pause
exit /b %RC%
