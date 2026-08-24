@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C1 Client Spell.dbc Rollback ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17C1-Client-MPQ.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C1] ROLLBACK FAILED.
) else (
  echo [G17C1] ROLLBACK PASSED. Client MPQ restored.
)
echo.
pause
exit /b %RC%
