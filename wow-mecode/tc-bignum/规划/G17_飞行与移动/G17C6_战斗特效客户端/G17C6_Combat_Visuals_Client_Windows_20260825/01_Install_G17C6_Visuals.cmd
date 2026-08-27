@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C6 Client Combat Visuals (patch Spell.dbc MPQ: visuals + 30yd ranges) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C6-Combat-Visuals.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C6] INSTALL FAILED.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17C6_CLIENT_VISUALS_RESULT.txt
) else (
  echo [G17C6] INSTALL PASSED. Restart the WoW client.
)
echo.
pause
exit /b %RC%
