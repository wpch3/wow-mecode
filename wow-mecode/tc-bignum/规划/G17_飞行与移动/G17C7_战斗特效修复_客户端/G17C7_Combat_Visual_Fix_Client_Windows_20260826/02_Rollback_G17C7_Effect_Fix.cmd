@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C7 Rollback ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17C7-Effect-Fix.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C7] ROLLBACK FAILED.
) else (
  echo [G17C7] ROLLBACK PASSED.
)
echo.
pause
exit /b %RC%
