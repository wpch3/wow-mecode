@echo off
setlocal
echo G17-R3 full rollback. Wow.exe and worldserver must both be closed.
set /p CONFIRM=Type ROLLBACK in uppercase to continue: 
if not "%CONFIRM%"=="ROLLBACK" exit /b 2
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17R3-Client-MPQ.ps1"
if errorlevel 1 goto fail
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17R3-Server.ps1"
if errorlevel 1 goto fail
echo G17-R3 full rollback PASS.
pause
exit /b 0
:fail
echo G17-R3 rollback stopped. Return the rollback result files; do not perform manual replacements.
pause
exit /b 1
