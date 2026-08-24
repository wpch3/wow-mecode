@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17R1-Windows.ps1"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo G17-R1 source apply and Windows build completed.
) else (
  echo G17-R1 stopped with error code %RC%.
)
echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17R1_WINDOWS_FIX_RESULT.txt
pause
exit /b %RC%
