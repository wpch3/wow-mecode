@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C8 Rollback ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17C8-PerSlot-Visuals.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C8] ROLLBACK FAILED.
) else (
  echo [G17C8] ROLLBACK PASSED.
)
echo.
pause
exit /b %RC%
