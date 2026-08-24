@echo off
setlocal
cd /d "%~dp0"
echo [G17-R4] Close Wow.exe before rollback.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17R4-Client-MPQ.ps1"
set RC=%ERRORLEVEL%
pause
exit /b %RC%
