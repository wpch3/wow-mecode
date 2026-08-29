@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B3R6 Performance Fix (remove mount lag + no cooldown without target) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17B3R8-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B3R8] FAILED.
) else (
  echo [G17B3R8] PASSED. Restart worldserver.
)
pause
exit /b %RC%
