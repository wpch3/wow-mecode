@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-G17R5-Locale-Mirror.ps1" -Workspace "C:\Users\Administrator\Downloads\workspace" -ClientRoot "D:\WOW"
echo.
echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17R5_LOCALE_MIRROR_ROLLBACK_RESULT.txt
if errorlevel 1 (echo G17R5 ROLLBACK FAILED&pause&exit /b 1)
echo G17R5 ROLLBACK PASS.
pause
