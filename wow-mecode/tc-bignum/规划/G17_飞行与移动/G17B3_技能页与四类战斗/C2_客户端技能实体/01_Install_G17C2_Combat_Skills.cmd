@echo off
setlocal
chcp 65001 >nul
echo.
echo === G17-C2 Combat Skills Client DBC (25 B3 skills appended) ===
echo.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-G17C2-Combat-Skills.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [G17C2] CLIENT COMBAT SKILLS FAILED. Do NOT start WoW.
  echo Result: C:\Users\Administrator\Downloads\workspace\uploads\G17C2_CLIENT_COMBAT_SKILLS_RESULT.txt
) else (
  echo [G17C2] CLIENT COMBAT SKILLS PASSED. Restart WoW.
)
echo.
pause
exit /b %RC%
