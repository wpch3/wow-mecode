@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C7 Combat Visual Effect Fix (Effect_1: DUMMY->SCHOOL_DAMAGE, client+server) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C7-Effect-Fix.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C7] INSTALL FAILED.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17C7_EFFECT_FIX_RESULT.txt
) else (
  echo [G17C7] INSTALL PASSED. Restart both worldserver and WoW client.
)
echo.
pause
exit /b %RC%
