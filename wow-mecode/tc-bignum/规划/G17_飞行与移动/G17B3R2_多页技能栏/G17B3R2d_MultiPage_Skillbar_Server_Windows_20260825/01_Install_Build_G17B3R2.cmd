@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B3R2 Multi-Page Vehicle Skill Bar (source + server DBC + SQL + rebuild) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17B3R2-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B3R2] INSTALL/BUILD FAILED. Do NOT start worldserver.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17B3R2_WINDOWS_BUILD_RESULT.txt
) else (
  echo [G17B3R2] INSTALL/BUILD PASSED.
  echo Start worldserver, look for "G17-B3R2 multi-page skillbar LOADED" in worldserver.log.
)
echo.
pause
exit /b %RC%
