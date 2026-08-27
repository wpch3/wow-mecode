@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C6 Rollback (restore client MPQ backups) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17C6-Combat-Visuals.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C6] ROLLBACK FAILED.
) else (
  echo [G17C6] ROLLBACK PASSED.
)
echo.
pause
exit /b %RC%
