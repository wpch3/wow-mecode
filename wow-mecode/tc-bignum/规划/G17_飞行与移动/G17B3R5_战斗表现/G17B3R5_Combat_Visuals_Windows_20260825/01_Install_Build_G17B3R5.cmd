@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B3R5 Combat Visuals (source + server DBC visuals + addon v3 + rebuild) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17B3R5-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B3R5] INSTALL/BUILD FAILED. Do NOT start worldserver.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17B3R5_WINDOWS_BUILD_RESULT.txt
) else (
  echo [G17B3R5] INSTALL/BUILD PASSED. Remember to also install G17C6 (client visuals).
)
echo.
pause
exit /b %RC%
