@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B3R1 Rollback (source floor + DBC restore) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-Build-G17B3R1-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B3R1] ROLLBACK FAILED.
) else (
  echo [G17B3R1] ROLLBACK PASSED.
)
echo.
pause
exit /b %RC%
