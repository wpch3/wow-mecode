@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C11 Rollback (restore pre-C11 client MPQs) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17C13-ClientDBC.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C13] ROLLBACK FAILED.
) else (
  echo [G17C13] ROLLBACK PASSED. Restart the WoW client.
)
pause
exit /b %RC%
