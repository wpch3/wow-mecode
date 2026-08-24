@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B2R2 Rollback to B2R1 floor + Rebuild ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-Build-G17B2R3-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B2R3] ROLLBACK FAILED.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17B2R3_WINDOWS_ROLLBACK_RESULT.txt
) else (
  echo [G17B2R3] ROLLBACK PASSED. Now at B2R1 floor.
)
echo.
pause
exit /b %RC%
