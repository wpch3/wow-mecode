@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B3R6 Rollback ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-Build-G17B3R7-Windows.ps1" %*
pause
exit /b %ERRORLEVEL%
