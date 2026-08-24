@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C1 Client Spell.dbc Unlock (52226 focus/aura cleared) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C1-Client-MPQ.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C1] CLIENT UNLOCK FAILED. Do NOT start WoW.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17C1_CLIENT_MPQ_UNLOCK_RESULT.txt
) else (
  echo [G17C1] CLIENT UNLOCK PASSED. Restart WoW and test skill 4.
)
echo.
pause
exit /b %RC%
