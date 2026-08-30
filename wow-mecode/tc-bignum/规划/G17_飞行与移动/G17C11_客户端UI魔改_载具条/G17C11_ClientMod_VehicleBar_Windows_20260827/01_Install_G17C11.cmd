@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C11 REAL CLIENT MOD: 8-slot stock vehicle action bar ===
echo BUILD=v1_vehiclebar_8slots
echo Patches the client's own FrameXML (VehicleMenuBar.lua/xml) via the MPQ chain.
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C11-ClientMod.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C11] INSTALL FAILED.
) else (
  echo [G17C11] INSTALL PASSED. Restart the WoW client.
)
pause
exit /b %RC%
