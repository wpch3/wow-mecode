@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C13 Client DBC: slot-7 completion + full impact visuals ===
echo BUILD=v1_slot7_impact_visuals
echo Appends 990029/990030 + flight visuals + 25 impact-kit combat visuals via the MPQ chain.
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C13-ClientDBC.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C13] INSTALL FAILED.
) else (
  echo [G17C13] INSTALL PASSED. Restart the WoW client.
)
pause
exit /b %RC%
