@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B3R3 DF-style Skill Panel (source + G17DragonBar addon + rebuild) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17B3R3-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B3R3] INSTALL/BUILD FAILED. Do NOT start worldserver.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17B3R3_WINDOWS_BUILD_RESULT.txt
) else (
  echo [G17B3R3] INSTALL/BUILD PASSED.
  echo Start worldserver + WoW client; summon a mount; the G17DragonBar appears while riding.
)
echo.
pause
exit /b %RC%
