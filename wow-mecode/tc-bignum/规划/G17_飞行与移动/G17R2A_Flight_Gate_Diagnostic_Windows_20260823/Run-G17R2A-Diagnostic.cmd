@echo off
setlocal
echo G17-R2A read-only client/server flight-gate diagnostic
echo Close Wow.exe after reproducing spell 59961 once. The server may remain running.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Probe-G17R2A-Flight-Gates.ps1"
set "RC=%ERRORLEVEL%"
echo.
echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17R2A_GATE_DIAGNOSTIC_RESULT.txt
if not "%RC%"=="0" echo Diagnostic failed; return the same result file without retrying old fixes.
pause
exit /b %RC%
