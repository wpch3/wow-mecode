@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C9 v3 Rollback (restore pre-C9 client MPQs) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17C9-Real-Visuals.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C9 v3] ROLLBACK FAILED.
) else (
  echo [G17C9 v3] ROLLBACK PASSED. Restart the WoW client.
)
pause
exit /b %RC%
