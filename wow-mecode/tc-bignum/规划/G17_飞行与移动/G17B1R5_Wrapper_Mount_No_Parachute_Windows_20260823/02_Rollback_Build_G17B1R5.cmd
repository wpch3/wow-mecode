@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-Build-G17B1R5-Windows.ps1" -Workspace "C:\Users\Administrator\Downloads\workspace" -SourceRoot "D:\TrinityCore" -BuildRoot "D:\TC-Build"
if errorlevel 1 (echo G17B1R5 ROLLBACK FAILED - DO NOT START WORLDSERVER&pause&exit /b 1)
echo G17B1R5 ROLLBACK BUILD PASS.
pause
