@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C3 v2 (v2_workroot_fix) Client Skill Page Buttons ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C3-Skill-Page-Buttons.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C3] INSTALL FAILED.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17C3_CLIENT_BAR_BUTTONS_RESULT.txt
) else (
  echo [G17C3 v2] INSTALL PASSED. Restart the WoW client.
)
echo.
pause
exit /b %RC%
