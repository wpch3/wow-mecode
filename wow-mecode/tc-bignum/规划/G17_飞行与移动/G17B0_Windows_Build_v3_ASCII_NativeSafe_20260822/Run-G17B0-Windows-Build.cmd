@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo [G17-B0] Windows RelWithDebInfo x64 controlled build
echo [G17-B0] worldserver must be stopped. This package does not start it.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-G17B0-Windows.ps1"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo [PASS] Upload C:\Users\Administrator\Downloads\workspace\uploads\G17B0_WINDOWS_BUILD_RESULT.txt
) else (
  echo [FAIL] Stop. Upload the same result TXT; do not start worldserver.
)
echo ExitCode=%RC%
pause
exit /b %RC%
