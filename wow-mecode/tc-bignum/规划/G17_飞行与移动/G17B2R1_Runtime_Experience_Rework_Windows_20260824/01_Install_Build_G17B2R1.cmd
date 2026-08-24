@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17B2R1-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" echo G17B2R1 install/build failed. Do not start worldserver. Check workspace\uploads\G17B2R1_WINDOWS_BUILD_RESULT.txt
if "%RC%"=="0" echo G17B2R1 install/build passed. You may start worldserver normally.
pause
exit /b %RC%
