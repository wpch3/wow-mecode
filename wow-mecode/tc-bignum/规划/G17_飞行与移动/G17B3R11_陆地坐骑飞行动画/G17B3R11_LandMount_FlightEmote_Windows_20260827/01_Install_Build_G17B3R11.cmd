@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B3R6 Performance Fix (remove mount lag + no cooldown without target) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17B3R11-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B3R11] FAILED.
) else (
  echo [G17B3R11] PASSED. Restart worldserver.
)
pause
exit /b %RC%
