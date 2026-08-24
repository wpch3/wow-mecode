@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B2R2 Install + World SQL + Rebuild worldserver ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17B2R3-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B2R3] INSTALL/BUILD FAILED. Do NOT start worldserver.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17B2R3_WINDOWS_BUILD_RESULT.txt
) else (
  echo [G17B2R3] INSTALL/BUILD PASSED.
  echo Start D:\TC-Build\bin\RelWithDebInfo\worldserver.exe, then press skills 2/3/4 once.
  echo Look for the banner "G17-B2R2 LOADED" near startup in worldserver.log.
  echo Send back: C:\Users\Administrator\Downloads\workspace\uploads\G17B2R3_WINDOWS_BUILD_RESULT.txt
)
echo.
pause
exit /b %RC%
