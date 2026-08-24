@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B2R4 Server Spell.dbc Rollback ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-Build-G17B2R4-Server-DBC.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B2R4] ROLLBACK FAILED.
) else (
  echo [G17B2R4] ROLLBACK PASSED. Server Spell.dbc restored.
)
echo.
pause
exit /b %RC%
