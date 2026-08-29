@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C10 All-Archetype Visuals (client) + G17DragonBar addon folder ===
echo BUILD=v1_all_archetypes_visuals
echo Dragon visuals UNCHANGED (user-verified); beast/magic/mech/generic refined.
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C10-All-Archetypes.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C10] INSTALL FAILED.
) else (
  echo [G17C10] INSTALL PASSED. Restart the WoW client.
  echo If you want the dedicated 8-slot dragon bar: copy addon\G17DragonBar
  echo to D:\WOW\Interface\AddOns\G17DragonBar and restart the client.
)
pause
exit /b %RC%
