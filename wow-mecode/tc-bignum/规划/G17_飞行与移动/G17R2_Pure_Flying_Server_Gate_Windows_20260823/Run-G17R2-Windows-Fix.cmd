@echo off
setlocal
echo G17-R2: pure-flight strict-location server fix and rebuild
echo This package does not repeat R1, SQL, or the client MPQ install.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17R2-Windows.ps1"
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo G17-R2 failed. Return uploads\G17R2_WINDOWS_FIX_RESULT.txt
) else (
  echo G17-R2 build passed. Start worldserver normally, then test spell 59961 outdoors in Wetlands.
)
echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17R2_WINDOWS_FIX_RESULT.txt
pause
exit /b %RC%
