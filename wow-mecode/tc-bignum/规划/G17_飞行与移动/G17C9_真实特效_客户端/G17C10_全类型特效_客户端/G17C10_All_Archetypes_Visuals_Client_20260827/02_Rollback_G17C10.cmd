@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C10 Rollback (restore pre-C10 client MPQs) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17C10-All-Archetypes.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C10] ROLLBACK FAILED.
) else (
  echo [G17C10] ROLLBACK PASSED. Restart the WoW client.
)
pause
exit /b %RC%
