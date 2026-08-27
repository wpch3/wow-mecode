@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C9 v3 Real Per-Skill Visuals + Remove Phantom Cooldown (client) ===
echo BUILD=v3_wowhead_visuals_no_dbc_cd
echo (v3 = same Wowhead-verified visuals as v2 + FIXED installer; v1/v2 could never pass)
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C9-Real-Visuals.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C9 v3] INSTALL FAILED.
) else (
  echo [G17C9 v3] INSTALL PASSED. Restart the WoW client.
)
pause
exit /b %RC%
