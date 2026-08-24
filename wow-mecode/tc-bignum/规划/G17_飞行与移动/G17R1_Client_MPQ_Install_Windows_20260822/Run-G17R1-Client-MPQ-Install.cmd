@echo off
setlocal
cd /d "%~dp0"
echo G17-R1 packed client MPQ installer
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17R1-Client-MPQ.ps1" -ClientRoot "D:\WOW"
set "RC=%ERRORLEVEL%"
echo.
echo ExitCode=%RC%
echo Result=C:\Users\Administrator\Downloads\workspace\uploads\G17R1_CLIENT_MPQ_INSTALL_RESULT.txt
pause
exit /b %RC%
