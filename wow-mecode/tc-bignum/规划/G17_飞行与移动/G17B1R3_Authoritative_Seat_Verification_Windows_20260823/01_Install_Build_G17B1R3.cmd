@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17B1R3-Windows.ps1" -Workspace "C:\Users\Administrator\Downloads\workspace" -SourceRoot "D:\TrinityCore" -BuildRoot "D:\TC-Build"
echo.
echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17B1R3_WINDOWS_BUILD_RESULT.txt
if errorlevel 1 (echo G17B1R3 FAILED - DO NOT START WORLDSERVER&pause&exit /b 1)
echo G17B1R3 APPLY AND BUILD PASS. Start worldserver and follow README_FIRST.txt.
pause
