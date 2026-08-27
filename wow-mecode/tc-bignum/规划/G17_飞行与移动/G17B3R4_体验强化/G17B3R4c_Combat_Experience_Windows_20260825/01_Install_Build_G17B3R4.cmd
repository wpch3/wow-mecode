@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B3R4 Combat Experience (source + server DBC range fix + addon v2 + rebuild) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17B3R4-Windows.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B3R4] INSTALL/BUILD FAILED. Do NOT start worldserver.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17B3R4_WINDOWS_BUILD_RESULT.txt
) else (
  echo [G17B3R4] INSTALL/BUILD PASSED.
  echo Start worldserver + WoW client; summon a mount; G17DragonBar v2 shows 11 buttons.
)
echo.
pause
exit /b %RC%
