@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C9 Rollback ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17C9-Real-Visuals.ps1" %*
pause
exit /b %ERRORLEVEL%
