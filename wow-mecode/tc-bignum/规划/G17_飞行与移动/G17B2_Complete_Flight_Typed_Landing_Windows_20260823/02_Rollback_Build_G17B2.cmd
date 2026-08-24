@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Rollback-Build-G17B2-Windows.ps1" -Workspace "C:\Users\Administrator\Downloads\workspace" -SourceRoot "D:\TrinityCore" -BuildRoot "D:\TC-Build"
if errorlevel 1 (echo G17B2 ROLLBACK FAILED&pause&exit /b 1)
echo G17B2 ROLLBACK BUILD PASS.
pause
