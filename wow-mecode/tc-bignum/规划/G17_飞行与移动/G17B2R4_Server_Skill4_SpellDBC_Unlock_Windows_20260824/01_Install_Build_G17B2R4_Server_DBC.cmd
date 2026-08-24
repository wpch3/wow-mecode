@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-B2R4 Server Spell.dbc Unlock (52226 focus/aura cleared) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Build-G17B2R4-Server-DBC.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17B2R4] SERVER DBC UNLOCK FAILED. Do NOT start worldserver.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17B2R4_SERVER_SPELL_DBC_RESULT.txt
) else (
  echo [G17B2R4] SERVER DBC UNLOCK PASSED.
  echo Now start D:\TC-Build\bin\RelWithDebInfo\worldserver.exe and test skill 4 in game.
)
echo.
pause
exit /b %RC%
