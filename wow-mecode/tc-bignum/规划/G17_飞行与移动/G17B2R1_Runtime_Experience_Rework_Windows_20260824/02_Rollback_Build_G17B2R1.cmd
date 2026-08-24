@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-Build-G17B2R1-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" echo G17B2R1 safety rollback/build failed. Do not start worldserver. Check workspace\uploads\G17B2R1_WINDOWS_ROLLBACK_RESULT.txt
if "%RC%"=="0" echo G17B2R1 safety rollback/build passed. Visual-free landing action remains enforced.
pause
exit /b %RC%
