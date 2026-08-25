@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C2 Combat Skills Rollback ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17C2-Combat-Skills.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C2] ROLLBACK FAILED.
) else (
  echo [G17C2] ROLLBACK PASSED.
)
echo.
pause
exit /b %RC%
